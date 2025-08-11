#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/resource.h>
#include <errno.h>

// 日志宏定义
#define LOG_TAG "KLAUNCH"
#define LOGI(...) printf("[INFO][" LOG_TAG "] " __VA_ARGS__); printf("\n")
#define LOGE(...) printf("[ERROR][" LOG_TAG "] " __VA_ARGS__); printf("\n")

// 无痕启动程序
bool launch_stealth(const std::string& executable_path) {
    pid_t pid = fork();
    
    if (pid == -1) {
        LOGE("Failed to fork process: %s", strerror(errno));
        return false;
    }
    
    if (pid == 0) {
        // 子进程
        
        // 创建新的会话，脱离父进程控制
        if (setsid() == -1) {
            LOGE("Failed to create new session: %s", strerror(errno));
            exit(1);
        }
        
        // 再次fork，确保完全脱离终端
        pid_t pid2 = fork();
        if (pid2 == -1) {
            LOGE("Failed to fork second time: %s", strerror(errno));
            exit(1);
        }
        
        if (pid2 > 0) {
            // 第一个子进程退出
            exit(0);
        }
        
        // 第二个子进程（孙进程）
        

        // 重定向标准输入输出到/dev/null
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd != -1) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            if (null_fd > 2) {
                close(null_fd);
            }
        }
        
        // 清理环境变量以减少痕迹
        clearenv();
        
        // 设置最低优先级以减少系统影响
        nice(19);
        
        // 忽略一些信号
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        
        // 执行目标程序 - 使用隐蔽的进程名
        // 将进程名伪装成系统进程
        const char* disguised_name = "[kthreadd]";
        execl(executable_path.c_str(), disguised_name, (char*)NULL);
        
        // 如果execl失败，程序会继续执行到这里
        LOGE("Failed to execute %s: %s", executable_path.c_str(), strerror(errno));
        exit(1);
    } else {
        // 父进程
        
        // 等待第一个子进程退出
        int status;
        waitpid(pid, &status, 0);
        
        LOGI("Successfully launched %s in stealth mode", executable_path.c_str());
        return true;
    }
}

// 检查文件是否存在且可执行
bool is_executable(const std::string& path) {
    return access(path.c_str(), F_OK | X_OK) == 0;
}

int main(int argc, char* argv[]) {
    LOGI("KLAUNCH v2.4 - Stealth Process Launcher");
    LOGI("Author: IDlike");
    
    if (argc != 2) {
        LOGE("Usage: %s <executable_path>", argv[0]);
        LOGE("Example: %s /system/bin/sh", argv[0]);
        return 1;
    }
    
    std::string executable_path = argv[1];
    
    // 检查文件是否存在且可执行
    if (!is_executable(executable_path)) {
        LOGE("File does not exist or is not executable: %s", executable_path.c_str());
        return 1;
    }
    
    LOGI("Launching: %s", executable_path.c_str());
    
    if (launch_stealth(executable_path)) {
        LOGI("Process launched successfully in stealth mode");
        return 0;
    } else {
        LOGE("Failed to launch process");
        return 1;
    }
}