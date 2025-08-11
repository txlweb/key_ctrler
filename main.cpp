// 最小化头文件包含以减少内存占用
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <linux/input.h>
#include <android/log.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstdarg>
#include <sys/resource.h>
#include <sched.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <android/log.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstdarg>
#include <sys/resource.h>
#include <sched.h>
#include <sys/mman.h>

#define LOG_TAG "KCTRL"
#define LOG_FILE "/data/adb/modules/kctrl/klog.log"

// 日志文件输出函数 - 内存优化版本
void write_log_to_file(const char* level, const char* format, ...) {
    // 使用静态缓冲区减少内存分配
    static char buffer[512];
    static char time_buffer[32];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // 简化时间格式以减少开销
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm* tm_info = localtime(&time_t);
    snprintf(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    // 使用C风格文件操作减少开销
    FILE* logfile = fopen(LOG_FILE, "a");
    if (logfile) {
        fprintf(logfile, "[%s][%s] %s\n", time_buffer, level, buffer);
        fclose(logfile);
    }
}

#define LOGI(...) do { \
    if (g_enable_log) { \
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__); \
        write_log_to_file("INFO", __VA_ARGS__); \
    } \
} while(0)

#define LOGW(...) do { \
    if (g_enable_log) { \
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__); \
        write_log_to_file("WARN", __VA_ARGS__); \
    } \
} while(0)

#define LOGE(...) do { \
    if (g_enable_log) { \
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); \
        write_log_to_file("ERROR", __VA_ARGS__); \
    } \
} while(0)

// 按键状态结构体 - 极致内存优化版本
struct KeyState {
    uint64_t press_time_ns;      // 纳秒时间戳，8字节
    uint64_t last_click_time_ns; // 纳秒时间戳，8字节
    uint8_t flags;               // 位域：bit0=is_pressed, bit1=timer_active, bit2-7=click_count
    std::thread timer_thread;
    
    KeyState() : press_time_ns(0), last_click_time_ns(0), flags(0) {}
    
    inline bool is_pressed() const { return flags & 1; }
    inline void set_pressed(bool pressed) { 
        flags = pressed ? (flags | 1) : (flags & 0xFE); 
    }
    inline bool timer_active() const { return flags & 2; }
    inline void set_timer_active(bool active) {
        flags = active ? (flags | 2) : (flags & 0xFD);
    }
    inline uint8_t click_count() const { return (flags >> 2) & 0x3F; }
    inline void set_click_count(uint8_t count) { 
        flags = (flags & 3) | ((count & 0x3F) << 2); 
    }
};

// 全局变量 - 极致内存优化版本
static std::atomic<bool> g_running{true};
static int g_wakelock_fd = -1;

// 使用预分配的小容量容器减少内存碎片
static std::unordered_map<std::string, std::string> g_config;
static std::unordered_map<int, KeyState> g_key_states;

// 合并互斥锁减少同步开销
static std::mutex g_global_mutex;
static std::condition_variable g_shutdown_cv;

// 时间配置参数（毫秒）
static int g_click_threshold = 200;
static int g_short_press_threshold = 500;
static int g_long_press_threshold = 1000;
static int g_double_click_interval = 300;

// 日志开关配置
static bool g_enable_log = false;

// 信号处理函数
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        {
            std::lock_guard<std::mutex> lock(g_global_mutex);
            g_running = false;
        }
        g_shutdown_cv.notify_all();
        LOGI("Received signal %d, shutting down...", sig);
    }
}

// 检查是否已经运行
bool check_single_instance() {
    const char* pidfile = "/data/adb/modules/kctrl/mpid.txt";
    int fd = open(pidfile, O_CREAT | O_WRONLY | O_EXCL, 0644);
    
    if (fd == -1) {
        if (errno == EEXIST) {
            // 检查PID文件中的进程是否还在运行
            std::ifstream file(pidfile);
            if (file.is_open()) {
                pid_t old_pid;
                file >> old_pid;
                file.close();
                
                // 检查进程是否存在
                if (kill(old_pid, 0) == 0) {
                    LOGE("Another instance is already running with PID: %d", old_pid);
                    return false;
                } else {
                    // 旧进程已死，删除PID文件
                    unlink(pidfile);
                }
            }
        } else {
            LOGE("Failed to create PID file: %s", strerror(errno));
            return false;
        }
    }
    
    // 写入当前进程PID
    pid_t current_pid = getpid();
    std::ofstream outfile(pidfile);
    if (outfile.is_open()) {
        outfile << current_pid << std::endl;
        outfile.close();
        LOGI("Process PID %d written to %s", current_pid, pidfile);
    } else {
        LOGE("Failed to write PID to file");
        if (fd != -1) close(fd);
        return false;
    }
    
    if (fd != -1) close(fd);
    return true;
}

// 获取WakeLock
bool acquire_wakelock() {
    const char* wakelock_path = "/sys/power/wake_lock";
    g_wakelock_fd = open(wakelock_path, O_WRONLY);
    
    if (g_wakelock_fd == -1) {
        LOGE("Failed to open wake_lock: %s", strerror(errno));
        return false;
    }
    
    const char* lock_name = "kctrl_wakelock";
    if (write(g_wakelock_fd, lock_name, strlen(lock_name)) == -1) {
        LOGE("Failed to acquire wake lock: %s", strerror(errno));
        close(g_wakelock_fd);
        g_wakelock_fd = -1;
        return false;
    }
    
    LOGI("Wake lock acquired successfully");
    return true;
}

// 释放WakeLock
void release_wakelock() {
    if (g_wakelock_fd != -1) {
        const char* wakeunlock_path = "/sys/power/wake_unlock";
        int fd = open(wakeunlock_path, O_WRONLY);
        if (fd != -1) {
            const char* lock_name = "kctrl_wakelock";
            write(fd, lock_name, strlen(lock_name));
            close(fd);
        }
        close(g_wakelock_fd);
        g_wakelock_fd = -1;
        LOGI("Wake lock released");
    }
}

// 读取配置文件 - 深度内存优化版本
bool load_config(const char* config_file) {
    // 清空现有配置，确保重新加载时不会累积旧配置
    g_config.clear();
    
    // 使用C风格文件操作减少内存开销
    FILE* file = fopen(config_file, "r");
    if (!file) {
        LOGE("Failed to open config file: %s", config_file);
        return false;
    }
    
    // 使用固定大小缓冲区避免动态分配
    static char line_buffer[256];
    static char key_buffer[64];
    static char value_buffer[192];
    
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        // 跳过空行和注释
        if (line_buffer[0] == '\n' || line_buffer[0] == '#') continue;
        
        // 查找等号分隔符
        char* eq_pos = strchr(line_buffer, '=');
        if (!eq_pos) continue;
        
        // 分离键值对
        size_t key_len = eq_pos - line_buffer;
        if (key_len >= sizeof(key_buffer)) continue;
        
        strncpy(key_buffer, line_buffer, key_len);
        key_buffer[key_len] = '\0';
        
        // 移除值末尾的换行符
        char* value_start = eq_pos + 1;
        size_t value_len = strlen(value_start);
        if (value_len > 0 && value_start[value_len - 1] == '\n') {
            value_start[value_len - 1] = '\0';
        }
        
        strncpy(value_buffer, value_start, sizeof(value_buffer) - 1);
        value_buffer[sizeof(value_buffer) - 1] = '\0';
        
        // 存储配置
        g_config.emplace(key_buffer, value_buffer);
        
        // 只对非脚本配置输出日志
        if (strncmp(key_buffer, "script_", 7) != 0) {
            LOGI("Config: %s = %s", key_buffer, value_buffer);
        }
        
        // 解析时间配置参数
        if (strcmp(key_buffer, "click_threshold") == 0) {
            g_click_threshold = atoi(value_buffer);
        } else if (strcmp(key_buffer, "short_press_threshold") == 0) {
            g_short_press_threshold = atoi(value_buffer);
        } else if (strcmp(key_buffer, "long_press_threshold") == 0) {
            g_long_press_threshold = atoi(value_buffer);
        } else if (strcmp(key_buffer, "double_click_interval") == 0) {
            g_double_click_interval = atoi(value_buffer);
        } else if (strcmp(key_buffer, "enable_log") == 0) {
            g_enable_log = (atoi(value_buffer) != 0);
        }
    }
    
    fclose(file);
    LOGI("Config loaded - Click: %dms, Short: %dms, Long: %dms, Double: %dms, Log: %s", 
         g_click_threshold, g_short_press_threshold, g_long_press_threshold, g_double_click_interval,
         g_enable_log ? "enabled" : "disabled");
    return true;
}

// 基于通配符的简单匹配（仅支持'*'）
static bool wildcard_match(const std::string& text, const std::string& pattern) {
    size_t t = 0, p = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == text[t])) {
            ++t; ++p; // 逐字符匹配
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match; // 尝试扩展'*'匹配
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

// 获取输入设备名称
static bool get_input_device_name(const std::string& device_path, std::string& out_name) {
    int fd = open(device_path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    char name[256] = {0};
    bool ok = ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0;
    close(fd);
    if (ok) out_name.assign(name);
    return ok;
}

// 从 /dev/input 直接枚举 event 设备并读取名称
struct InputDevInfo { std::string path; std::string name; };
static std::vector<InputDevInfo> enumerate_event_devices_via_dev() {
    std::vector<InputDevInfo> out;
    DIR* dir = opendir("/dev/input");
    if (!dir) return out;
    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        if (strncmp(de->d_name, "event", 5) == 0) {
            std::string path = std::string("/dev/input/") + de->d_name;
            std::string name;
            if (get_input_device_name(path, name)) {
                out.push_back({path, name});
            } else {
                out.push_back({path, std::string()});
            }
        }
    }
    closedir(dir);
    return out;
}

// 解析 /proc/bus/input/devices 获取名称与 event 映射（fallback）
static std::vector<InputDevInfo> enumerate_event_devices_via_proc() {
    std::vector<InputDevInfo> out;
    std::ifstream fin("/proc/bus/input/devices");
    if (!fin.is_open()) return out;
    std::string line;
    std::string cur_name;
    while (std::getline(fin, line)) {
        if (line.rfind("N:", 0) == 0 || line.find("Name=") != std::string::npos) {
            auto pos = line.find("Name=");
            if (pos != std::string::npos) {
                std::string val = line.substr(pos + 5);
                // 去掉引号
                if (!val.empty() && (val.front()=='\"' || val.front()=='\'')) {
                    char q = val.front();
                    if (val.back()==q && val.size()>=2) val = val.substr(1, val.size()-2);
                }
                cur_name = val;
            }
        } else if (line.rfind("H:", 0) == 0 || line.find("Handlers=") != std::string::npos) {
            // 提取 eventX
            size_t start = 0;
            while (start < line.size()) {
                while (start < line.size() && line[start] == ' ') ++start;
                size_t end = start;
                while (end < line.size() && line[end] != ' ') ++end;
                if (end > start) {
                    std::string tok = line.substr(start, end - start);
                    if (tok.rfind("event", 0) == 0) {
                        out.push_back({std::string("/dev/input/") + tok, cur_name});
                    }
                }
                start = end + 1;
            }
            cur_name.clear();
        }
    }
    return out;
}

// 综合枚举：优先 /dev/input，其次 /proc/bus/input/devices
static std::vector<InputDevInfo> enumerate_all_event_devices() {
    auto via_dev = enumerate_event_devices_via_dev();
    if (!via_dev.empty()) return via_dev;
    auto via_proc = enumerate_event_devices_via_proc();
    return via_proc;
}

// 解析设备配置：支持绝对路径、event编号以及名称/通配符（带 fallback）
static std::vector<std::string> resolve_device_config(const std::string& devices_config) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : devices_config) {
        if (c == '|') { if (!cur.empty()) { tokens.push_back(cur); cur.clear(); } }
        else { cur += c; }
    }
    if (!cur.empty()) tokens.push_back(cur);

    std::vector<std::string> resolved;
    auto add_unique = [&resolved](const std::string& path){ for (auto& p : resolved) if (p == path) return; resolved.push_back(path); };

    auto all_events = enumerate_all_event_devices();
    if (all_events.empty()) {
        LOGW("No /dev/input events found via both /dev and /proc; name matching may fail");
    } else {
        for (const auto& info : all_events) {
            LOGI("Enumerated input: %s -> %s", info.path.c_str(), info.name.empty()?"<unknown>":info.name.c_str());
        }
    }

    for (auto token : tokens) {
        // 去空格
        while (!token.empty() && (token.front()==' '||token.front()=='\t')) token.erase(token.begin());
        while (!token.empty() && (token.back()==' '||token.back()=='\t')) token.pop_back();
        if (token.empty()) continue;

        // 可选前缀
        if (token.rfind("name:", 0) == 0) token = token.substr(5);
        else if (token.rfind("name=", 0) == 0) token = token.substr(5);
        // 去引号
        if (token.size() >= 2 && ((token.front()=='\"' && token.back()=='\"') || (token.front()=='\'' && token.back()=='\''))) {
            token = token.substr(1, token.size()-2);
        }

        // 1) 绝对路径
        if (!token.empty() && token[0] == '/') { add_unique(token); continue; }
        // 2) event编号
        if (token.rfind("event", 0) == 0) { add_unique(std::string("/dev/input/") + token); continue; }
        // 3) 名称或通配符
        for (const auto& info : all_events) {
            const std::string& name = info.name;
            if (!name.empty()) {
                if (token.find('*') != std::string::npos) {
                    if (wildcard_match(name, token)) add_unique(info.path);
                } else {
                    if (name == token) add_unique(info.path);
                }
            }
        }
    }
    return resolved;
}

// 执行shell脚本 - 内存优化版本
void execute_script(const std::string& script_name, const std::string& event_type) {
    // 使用静态缓冲区避免动态分配
    static char command_buffer[512];
    
    // 直接构建命令字符串
    snprintf(command_buffer, sizeof(command_buffer), 
             "sh /data/adb/modules/kctrl/scripts/%s %s", 
             script_name.c_str(), event_type.c_str());
    
    LOGI("Executing: %s", command_buffer);
    
    int result = system(command_buffer);
    if (result == -1) {
        LOGE("Failed to execute script: %s", strerror(errno));
    } else {
        LOGI("Script executed with result: %d", result);
    }
}

// 处理按键事件类型识别 - 内存优化版本
void handle_key_event_type(int keycode, const std::string& event_type, int duration_ms = 0) {
    // 每次事件触发前重新加载配置文件，实现实时更新
    const char* config_file = "/data/adb/modules/kctrl/config.txt";
    if (!load_config(config_file)) {
        LOGE("Failed to reload config file before event handling");
        return;
    }
    
    // 使用静态缓冲区避免重复分配
    static char script_key_buffer[64];
    
    // 直接构建脚本键名，避免字符串拼接
    const char* suffix;
    if (event_type == "click") {
        suffix = "_click";
    } else if (event_type == "double_click") {
        suffix = "_double_click";
    } else if (event_type == "short_press") {
        suffix = "_short_press";
    } else if (event_type == "long_press") {
        suffix = "_long_press";
    } else {
        return; // 不支持的事件类型
    }
    
    snprintf(script_key_buffer, sizeof(script_key_buffer), "script_%d%s", keycode, suffix);
    
    auto it = g_config.find(script_key_buffer);
    if (it != g_config.end()) {
        // 移除文件日志记录，直接执行脚本
        std::thread script_thread(execute_script, it->second, event_type);
        script_thread.detach();
    }
}

// 处理双击检测定时器 - 内存优化版本
void double_click_timer(int keycode) {
    std::this_thread::sleep_for(std::chrono::milliseconds(g_double_click_interval));
    
    // 减少锁持有时间，先读取状态再处理
    uint8_t click_count;
    bool timer_active;
    
    {
        std::lock_guard<std::mutex> lock(g_global_mutex);
        auto& state = g_key_states[keycode];
        click_count = state.click_count();
        timer_active = state.timer_active();
        
        if (timer_active) {
            state.set_click_count(0);
            state.set_timer_active(false);
        }
    }
    
    // 在锁外处理事件，减少锁竞争
    if (timer_active) {
        if (click_count == 1) {
            handle_key_event_type(keycode, "click");
        } else if (click_count >= 2) {
            handle_key_event_type(keycode, "double_click");
        }
    }
}

// 处理按键释放后的事件判断定时器 - 内存优化版本
void key_release_timer(int keycode, long long duration) {
    // 直接处理事件，无需锁定状态
    if (duration >= g_long_press_threshold) {
        // 长按事件
        handle_key_event_type(keycode, "long_press", duration);
    } else if (duration > g_click_threshold) {
        // 短按事件
        handle_key_event_type(keycode, "short_press", duration);
    }
    // 点击事件在双击检测定时器中处理
}

// 监听输入设备事件
void monitor_input_device(const std::string& device_path) {
    int fd = open(device_path.c_str(), O_RDONLY);
    if (fd == -1) {
        LOGE("Failed to open input device %s: %s", device_path.c_str(), strerror(errno));
        return;
    }
    
    LOGI("Monitoring input device: %s", device_path.c_str());
    
    struct input_event ev;
    while (g_running) {
        ssize_t bytes = read(fd, &ev, sizeof(ev));
        if (bytes == sizeof(ev)) {
            // 只处理按键事件
            if (ev.type == EV_KEY) {
                if (ev.value == 1) {
                    // 按键按下
                    std::lock_guard<std::mutex> lock(g_global_mutex);
                    auto& state = g_key_states[ev.code];
                    
                    state.set_pressed(true);
                    state.press_time_ns = std::chrono::steady_clock::now().time_since_epoch().count();
                    
                    LOGI("Key pressed: %d", ev.code);
                    
                    // 按下时不触发任何脚本，等待释放时判断事件类型
                    
                } else if (ev.value == 0) {
                    // 按键释放
                    std::lock_guard<std::mutex> lock(g_global_mutex);
                    auto& state = g_key_states[ev.code];
                    
                    if (state.is_pressed()) {
                         state.set_pressed(false);
                         auto release_time_ns = std::chrono::steady_clock::now().time_since_epoch().count();
                         int duration = static_cast<int>((release_time_ns - state.press_time_ns) / 1000000); // 转换为毫秒
                        
                        LOGI("Key released: %d (duration: %dms)", ev.code, duration);
                        
                        // 判断事件类型
                        if (duration <= g_click_threshold) {
                            // 点击事件 - 需要检测单击/双击
                            state.set_click_count(state.click_count() + 1);
                            state.last_click_time_ns = release_time_ns;
                            
                            if (!state.timer_active()) {
                                state.set_timer_active(true);
                                if (state.timer_thread.joinable()) {
                                    state.timer_thread.detach();
                                }
                                state.timer_thread = std::thread(double_click_timer, ev.code);
                            }
                        } else {
                            // 短按或长按事件 - 使用统一的定时器处理
                            if (state.timer_thread.joinable()) {
                                state.timer_thread.detach();
                            }
                            state.timer_thread = std::thread(key_release_timer, ev.code, duration);
                        }
                        // 按键释放时不触发keyup事件
                    }
                } else {
                    continue; // 忽略重复事件
                }
            }
        } else if (bytes == -1) {
            if (errno == EINTR) continue;
            LOGE("Error reading from input device: %s", strerror(errno));
            break;
        }
    }
    
    close(fd);
    LOGI("Stopped monitoring input device: %s", device_path.c_str());
}

// 清理函数
void cleanup() {
    g_running = false;
    
    // 清理按键状态和线程
    {
        std::lock_guard<std::mutex> lock(g_global_mutex);
        for (auto& pair : g_key_states) {
            auto& state = pair.second;
            state.set_pressed(false);
            state.set_timer_active(false);
            if (state.timer_thread.joinable()) {
                state.timer_thread.detach();
            }
        }
        g_key_states.clear();
    }
    
    // 恢复系统资源设置
    munlockall(); // 解锁内存，允许使用swap
    
    // 恢复CPU亲和性到所有核心
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); i++) {
        CPU_SET(i, &cpu_set);
    }
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    
    release_wakelock();
    unlink("/data/adb/modules/kctrl/mpid.txt");
    LOGI("Cleanup completed with system resources restored");
}

int main(int argc, char* argv[]) {
    LOGI("KCTRL v2.4 starting...");
    LOGI("Author: IDlike");
    LOGI("Description: 适用于Android15+的按键控制模块");
    
    // 设置信号处理
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // 系统资源优化设置
    // 1. 降低CPU优先级（nice值越高优先级越低，范围-20到19）
    if (setpriority(PRIO_PROCESS, 0, 10) == 0) {
        LOGI("CPU priority lowered to nice=10");
    } else {
        LOGW("Failed to lower CPU priority");
    }
    
    // 2. 设置CPU亲和性为单核心（使用CPU0）
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(0, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) == 0) {
        LOGI("CPU affinity set to core 0");
    } else {
        LOGW("Failed to set CPU affinity");
    }
    
    // 3. 设置内存策略优先使用swap
    // 确保内存不被锁定，允许系统将进程内存交换到swap
    munlockall();
    
    // 建议内核优先回收此进程的内存页面
    if (madvise(nullptr, 0, MADV_DONTNEED) == 0) {
        LOGI("Memory policy set to prefer swap usage");
    } else {
        LOGI("Memory unlocked, will use swap when needed");
    }
    
    // 4. 极致内存优化设置
    // 预分配容器以最小容量减少内存碎片
    g_config.reserve(8);  // 最小配置项数量
    g_key_states.reserve(4);  // 最多监听4个按键
    
    // 设置内存映射建议，优先回收不活跃页面
    if (madvise(nullptr, 0, MADV_SEQUENTIAL) == 0) {
        LOGI("Memory access pattern optimized for sequential access");
    }
    
    // 检查单实例运行
    if (!check_single_instance()) {
        LOGE("Another instance is already running");
        return 1;
    }
    
    // 获取WakeLock
    if (!acquire_wakelock()) {
        LOGE("Failed to acquire wake lock");
        cleanup();
        return 1;
    }
    
    // 加载配置文件
    const char* config_file = (argc > 1) ? argv[1] : "/data/adb/modules/kctrl/config.txt";
    if (!load_config(config_file)) {
        LOGE("Failed to load config file");
        cleanup();
        return 1;
    }
    
    // 获取要监听的设备路径
    auto device_it = g_config.find("device");
    if (device_it == g_config.end()) {
        LOGE("No device specified in config file");
        cleanup();
        return 1;
    }

    std::string devices_config = device_it->second;
    LOGI("Device config: %s", devices_config.c_str());

    // 新增：支持通过设备名称/通配符解析为实际路径
    std::vector<std::string> device_paths = resolve_device_config(devices_config);

    if (device_paths.empty()) {
        LOGE("No valid device paths found");
        cleanup();
        return 1;
    }

    LOGI("Found %zu device(s) to monitor", device_paths.size());
    for (const auto& path : device_paths) {
        LOGI("Target device: %s", path.c_str());
    }

    // 为每个设备启动独立的监听线程
    std::vector<std::thread> monitor_threads;
    monitor_threads.reserve(device_paths.size());

    for (const auto& device_path : device_paths) {
        monitor_threads.emplace_back(monitor_input_device, device_path);
    }
    
    // 主循环 - 使用条件变量优化响应性和定期内存回收
    {
        std::unique_lock<std::mutex> lock(g_global_mutex);
        while (g_running) {
            // 等待5分钟或直到程序退出
            if (g_shutdown_cv.wait_for(lock, std::chrono::minutes(5), []{ return !g_running; })) {
                break; // 程序退出
            }
            
            // 定期内存优化（每5分钟执行一次）
            if (g_running) {
                // 建议内核回收不活跃的内存页面
                madvise(nullptr, 0, MADV_DONTNEED);
                
                // 清理可能的内存碎片
                g_config.rehash(0);
                g_key_states.rehash(0);
                
                LOGI("Periodic memory optimization completed");
            }
        }
    }
    
    // 等待所有监听线程结束
    for (auto& thread : monitor_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    cleanup();
    LOGI("KCTRL stopped");
    return 0;
}