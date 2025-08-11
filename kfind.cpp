#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <linux/input.h>
#include <android/log.h>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <dirent.h>
#include <sys/ioctl.h>

#define LOG_TAG "KFIND"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// 全局变量
static std::atomic<bool> g_running{true};
static std::map<std::string, std::string> g_config;
static std::string g_config_dir;
static std::mutex g_output_mutex;
static std::condition_variable g_shutdown_cv;

// 信号处理函数
void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running = false;
        LOGI("Received signal %d, shutting down...", sig);
        {
            std::lock_guard<std::mutex> lock(g_output_mutex);
            std::cout << "\nShutting down..." << std::endl;
        }
        g_shutdown_cv.notify_all();
    }
}

// 读取配置文件
bool load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOGE("Failed to open config file: %s", config_file.c_str());
        std::cerr << "Error: Cannot open config file: " << config_file << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 查找等号分隔符
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            g_config[key] = value;
        }
    }
    
    file.close();
    LOGI("Config loaded successfully");
    return true;
}

// 获取按键名称
std::string get_key_name(int keycode) {
    static std::map<int, std::string> key_names = {
        {KEY_POWER, "POWER"},
        {KEY_VOLUMEUP, "VOLUME_UP"},
        {KEY_VOLUMEDOWN, "VOLUME_DOWN"},
        {KEY_HOME, "HOME"},
        {KEY_BACK, "BACK"},
        {KEY_MENU, "MENU"},
        {KEY_SEARCH, "SEARCH"},
        {KEY_CAMERA, "CAMERA"},
        // {KEY_FOCUS, "FOCUS"}, // Not available in this Android API level
        {KEY_ESC, "ESC"},
        {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"}, {KEY_5, "5"},
        {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"}, {KEY_0, "0"},
        {KEY_A, "A"}, {KEY_B, "B"}, {KEY_C, "C"}, {KEY_D, "D"}, {KEY_E, "E"},
        {KEY_F, "F"}, {KEY_G, "G"}, {KEY_H, "H"}, {KEY_I, "I"}, {KEY_J, "J"},
        {KEY_K, "K"}, {KEY_L, "L"}, {KEY_M, "M"}, {KEY_N, "N"}, {KEY_O, "O"},
        {KEY_P, "P"}, {KEY_Q, "Q"}, {KEY_R, "R"}, {KEY_S, "S"}, {KEY_T, "T"},
        {KEY_U, "U"}, {KEY_V, "V"}, {KEY_W, "W"}, {KEY_X, "X"}, {KEY_Y, "Y"},
        {KEY_Z, "Z"},
        {KEY_ENTER, "ENTER"},
        {KEY_SPACE, "SPACE"},
        {KEY_TAB, "TAB"},
        {KEY_LEFTSHIFT, "LEFT_SHIFT"},
        {KEY_RIGHTSHIFT, "RIGHT_SHIFT"},
        {KEY_LEFTCTRL, "LEFT_CTRL"},
        {KEY_RIGHTCTRL, "RIGHT_CTRL"},
        {KEY_LEFTALT, "LEFT_ALT"},
        {KEY_RIGHTALT, "RIGHT_ALT"},
        {KEY_UP, "UP"},
        {KEY_DOWN, "DOWN"},
        {KEY_LEFT, "LEFT"},
        {KEY_RIGHT, "RIGHT"}
    };
    
    auto it = key_names.find(keycode);
    if (it != key_names.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

// 监听单个输入设备
void monitor_input_device(const std::string& device_path) {
    int fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        LOGE("Failed to open device: %s - %s", device_path.c_str(), strerror(errno));
        std::cerr << "Error: Cannot open device " << device_path << ": " << strerror(errno) << std::endl;
        std::cerr << "Make sure you have root permissions and the device path is correct." << std::endl;
        return;
    }
    
    LOGI("Monitoring device: %s", device_path.c_str());
    {
        std::lock_guard<std::mutex> lock(g_output_mutex);
        std::cout << "Monitoring device: " << device_path << std::endl;
    }
    
    struct input_event event;
    
    while (g_running) {
        ssize_t bytes = read(fd, &event, sizeof(event));
        if (bytes != sizeof(event)) {
            if (bytes == -1) {
                if (errno == EINTR) {
                    if (!g_running) break; // 检查是否需要退出
                    continue; // 被信号中断，继续读取
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下没有数据可读，短暂休眠后继续
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                } else {
                    LOGE("Failed to read event: %s", strerror(errno));
                    break;
                }
            } else {
                // 读取的字节数不正确
                continue;
            }
        }
        
        // 再次检查g_running状态
        if (!g_running) {
            break;
        }
        
        // 只处理按键事件
            if (event.type == EV_KEY) {
                std::string key_name = get_key_name(event.code);
                std::string event_type;
                
                switch (event.value) {
                    case 0:
                        event_type = "RELEASE";
                        break;
                    case 1:
                        event_type = "PRESS";
                        break;
                    case 2:
                        event_type = "REPEAT";
                        break;
                    default:
                        event_type = "UNKNOWN";
                        break;
                }
                
                // 线程安全的输出和文件写入
                {
                    std::lock_guard<std::mutex> lock(g_output_mutex);
                    
                    // 输出按键信息
                    std::cout << "[" << event.code << "] " << key_name 
                              << " - " << event_type << " (" << event.value << ") [" << device_path << "]" << std::endl;
                    
                    // 记录到日志
                    LOGI("Key event: code=%d, name=%s, type=%s, value=%d, device=%s", 
                         event.code, key_name.c_str(), event_type.c_str(), event.value, device_path.c_str());
                    
                    // 写入到kfind.txt文件
                    std::string output_path = g_config_dir + "kfind.txt";
                    FILE* output_file = fopen(output_path.c_str(), "w");
                    if (output_file) {
                        fprintf(output_file, "[%d] %s - %s (%d) [%s]\n", 
                                event.code, key_name.c_str(), event_type.c_str(), event.value, device_path.c_str());
                        fclose(output_file);
                    }
                    
                    // 如果是按键释放事件，表示一次完整的按下抬起操作结束，退出程序
                    if (event.value == 0) {
                        std::cout << "检测到完整按键操作，程序即将退出..." << std::endl;
                        LOGI("Complete key press-release detected, shutting down...");
                        g_running = false;
                        g_shutdown_cv.notify_all();
                        close(fd);
                        LOGI("Stopped monitoring device: %s", device_path.c_str());
                        return;
                    }
                }
            }
    }
    
    close(fd);
    LOGI("Stopped monitoring device: %s", device_path.c_str());
}

// 基于通配符的简单匹配（仅支持'*'）
static bool wildcard_match_kf(const std::string& text, const std::string& pattern) {
    size_t t = 0, p = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == text[t])) { ++t; ++p; }
        else if (p < pattern.size() && pattern[p] == '*') { star = p++; match = t; }
        else if (star != std::string::npos) { p = star + 1; t = ++match; }
        else { return false; }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

// 获取输入设备名称
static bool get_input_device_name_kf(const std::string& device_path, std::string& out_name) {
    int fd = open(device_path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    char name[256] = {0};
    bool ok = ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0;
    close(fd);
    if (ok) out_name.assign(name);
    return ok;
}

// 枚举 /dev/input 下的 event 设备
static std::vector<std::string> list_event_devices_kf() {
    std::vector<std::string> devices;
    DIR* dir = opendir("/dev/input");
    if (!dir) return devices;
    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        if (strncmp(de->d_name, "event", 5) == 0) {
            devices.emplace_back(std::string("/dev/input/") + de->d_name);
        }
    }
    closedir(dir);
    return devices;
}

// 解析设备配置：支持绝对路径、event编号以及名称/通配符
static std::vector<std::string> resolve_device_config_kf(const std::string& devices_config) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : devices_config) {
        if (c == '|') { if (!cur.empty()) { tokens.push_back(cur); cur.clear(); } }
        else { cur += c; }
    }
    if (!cur.empty()) tokens.push_back(cur);

    std::vector<std::string> resolved;
    auto add_unique = [&resolved](const std::string& path){ for (auto& p : resolved) if (p == path) return; resolved.push_back(path); };

    auto all_events = list_event_devices_kf();

    for (auto token : tokens) {
        // 去空格
        while (!token.empty() && (token.front()==' '||token.front()=='\t')) token.erase(token.begin());
        while (!token.empty() && (token.back()==' '||token.back()=='\t')) token.pop_back();
        if (token.empty()) continue;

        // 可选前缀
        if (token.rfind("name:", 0) == 0) token = token.substr(5);
        else if (token.rfind("name=", 0) == 0) token = token.substr(5);
        // 去引号
        if (token.size() >= 2 && ((token.front()=='"' && token.back()=='"') || (token.front()=='\'' && token.back()=='\''))) {
            token = token.substr(1, token.size()-2);
        }

        // 1) 绝对路径
        if (!token.empty() && token[0] == '/') { add_unique(token); continue; }
        // 2) event编号
        if (token.rfind("event", 0) == 0) { add_unique(std::string("/dev/input/") + token); continue; }
        // 3) 名称或通配符
        for (const auto& path : all_events) {
            std::string name;
            if (get_input_device_name_kf(path, name)) {
                if (token.find('*') != std::string::npos) {
                    if (wildcard_match_kf(name, token)) add_unique(path);
                } else {
                    if (name == token) add_unique(path);
                }
            }
        }
    }
    return resolved;
}

int main(int argc, char* argv[]) {
    LOGI("KFIND v2.4 started - Key finder utility");
    LOGI("Author: IDlike");
    std::cout << "KFIND v2.4 - Key Finder Utility" << std::endl;
    std::cout << "Author: IDlike" << std::endl;
    std::cout << "================================" << std::endl;
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 确定配置文件路径
    std::string config_file = "/data/adb/modules/kctrl/config.txt";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    // 设置配置文件目录
    size_t last_slash = config_file.find_last_of('/');
    if (last_slash != std::string::npos) {
        g_config_dir = config_file.substr(0, last_slash + 1);
    } else {
        g_config_dir = "./";
    }
    
    // 加载配置
    if (!load_config(config_file)) {
        std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
        std::cerr << "Default config file: /data/adb/modules/kctrl/config.txt" << std::endl;
        return 1;
    }
    
    // 获取设备路径
    auto device_it = g_config.find("device");
    if (device_it == g_config.end()) {
        LOGE("Device path not found in config");
        std::cerr << "Error: 'device' not found in config file" << std::endl;
        std::cerr << "Please add a line like: device=/dev/input/event0" << std::endl;
        return 1;
    }
    
    std::string devices_config = device_it->second;
    LOGI("Device config: %s", devices_config.c_str());

    // 新增：名称/通配符解析
    std::vector<std::string> device_paths = resolve_device_config_kf(devices_config);

    if (device_paths.empty()) {
        LOGE("No valid device paths found");
        std::cerr << "Error: No valid device paths found" << std::endl;
        return 1;
    }

    LOGI("Found %zu device(s) to monitor", device_paths.size());
    std::cout << "Found " << device_paths.size() << " device(s) to monitor:" << std::endl;
    for (const auto& path : device_paths) {
        std::cout << "  - " << path << std::endl;
        LOGI("Target device: %s", path.c_str());
        if (access(path.c_str(), R_OK) != 0) {
            LOGW("Cannot access device: %s", path.c_str());
            std::cerr << "Warning: Cannot access device " << path << std::endl;
        }
    }

    // 为每个设备启动独立的监听线程
    std::vector<std::thread> monitor_threads;
    monitor_threads.reserve(device_paths.size());
    for (const auto& device_path : device_paths) {
        monitor_threads.emplace_back(monitor_input_device, device_path);
    }
    
    // 主循环 - 等待程序退出
    {
        std::unique_lock<std::mutex> lock(g_output_mutex);
        g_shutdown_cv.wait(lock, []{ return !g_running; });
    }
    
    // 等待所有监听线程结束
    for (auto& thread : monitor_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    std::cout << "KFIND stopped." << std::endl;
    LOGI("KFIND stopped");
    return 0;
}