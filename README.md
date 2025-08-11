# KCTRL - Android按键控制器

一个使用Android NDK 28为Android 15开发的二进制文件，用于监听设备按键事件并执行相应的shell脚本。

相应APP在 https://github.com/txlweb/key_ctrler_APP

## 功能特性

- ✅ **单实例运行保护** - 防止多次运行，PID输出到 `./mpid.txt`
- ✅ **WakeLock保活** - 使用系统级WakeLock确保锁屏状态下保持运行
- ✅ **底层按键监听** - 直接监听 `/dev/input/eventX` 设备事件
- ✅ **配置文件驱动** - 通过配置文件指定监听设备和对应脚本
- ✅ **智能按键识别** - 支持单击/双击/短按/长按事件识别
- ✅ **可配置时间阈值** - 所有时间参数可通过配置文件修改并即时生效
- ✅ **多事件类型支持** - 区分click、double_click、short_press、long_press事件
- ✅ **兼容性保持** - 保持对原有keydown/keyup事件的兼容
- ✅ **脚本执行** - 触发事件时异步执行对应的shell脚本
- ✅ **多架构支持** - 支持 arm64-v8a, armeabi-v7a, x86_64, x86

## 项目结构

```
KCTRL_CPP/
├── main.cpp              # 主程序源代码
├── CMakeLists.txt         # CMake构建配置
├── Android.mk             # NDK构建配置
├── Application.mk         # NDK应用配置
├── config.txt             # 配置文件示例
├── build.sh               # Linux/macOS构建脚本
├── build.bat              # Windows构建脚本
├── deploy.sh              # Linux/macOS部署脚本
├── deploy.bat             # Windows部署脚本
├── scripts/               # 示例脚本目录
│   ├── power_key.sh       # 电源键脚本
│   ├── volume_up.sh       # 音量加键脚本
│   ├── volume_down.sh     # 音量减键脚本
│   ├── home_key.sh        # Home键脚本
│   └── back_key.sh        # 返回键脚本
└── README.md              # 项目说明文档
```

## 环境要求

### 开发环境
- Android NDK r28 或更高版本
- CMake 3.18.1 或更高版本
- Android SDK Platform Tools (包含adb)

### 目标设备
- Android 15 (API Level 35) 或更高版本
- Root权限（用于访问输入设备和WakeLock）

## 构建说明

### 1. 设置环境变量

**Linux/macOS:**
```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk-r28
```

**Windows:**
```cmd
set ANDROID_NDK_ROOT=C:\path\to\android-ndk-r28
```

### 2. 编译二进制文件

**Linux/macOS:**
```bash
# 默认编译arm64-v8a架构
./build.sh

# 编译特定架构
./build.sh armeabi-v7a
./build.sh x86_64
./build.sh x86
```

**Windows:**
```cmd
REM 默认编译arm64-v8a架构
build.bat

REM 编译特定架构
build.bat armeabi-v7a
build.bat x86_64
build.bat x86
```

编译完成后会生成两个可执行文件：
- `kctrl`: 主程序，用于监听按键事件并执行相应脚本
- `kfind`: 按键检测工具，用于查找按键码


## 配置文件说明

配置文件 `config.txt` 格式如下：

```
# 输入设备路径
input_device=/dev/input/event2

# 时间阈值配置 (单位: 毫秒)
# 0-500ms视为按键点击
click_threshold=500
# 小于1000ms认为是短按
short_press_threshold=1000
# 大于2000ms认为是长按
long_press_threshold=2000
# 双击间隔时间
double_click_interval=300

# 按键事件对应的脚本路径
# 格式: 按键码_事件类型=脚本路径
# 事件类型支持:
#   - keydown: 按键按下 (兼容模式)
#   - keyup: 按键抬起 (兼容模式)
#   - click: 单击 (0-500ms)
#   - double_click: 双击 (两次点击间隔<300ms)
#   - short_press: 短按 (500ms-1000ms)
#   - long_press: 长按 (>2000ms)

# 电源键 (116)
116_keydown=./scripts/power_down.sh
116_keyup=./scripts/power_up.sh
116_click=./scripts/power_click.sh
116_double_click=./scripts/power_double.sh
116_short_press=./scripts/power_short.sh
116_long_press=./scripts/power_long.sh

# 音量减键 (114)
114_keydown=./scripts/volume_down.sh
114_click=./scripts/volume_down_click.sh
114_long_press=./scripts/volume_down_long.sh

# 音量加键 (115)
115_keydown=./scripts/volume_up.sh
115_click=./scripts/volume_up_click.sh
115_long_press=./scripts/volume_up_long.sh

# Home键 (102)
102_keydown=./scripts/home_down.sh
102_double_click=./scripts/home_double.sh

# 返回键 (158)
158_keydown=./scripts/back_down.sh
158_long_press=./scripts/back_long.sh
```

### 常见按键代码

| 按键 | 代码 | 说明 |
|------|------|------|
| 电源键 | 116 | KEY_POWER |
| 音量减 | 114 | KEY_VOLUMEDOWN |
| 音量加 | 115 | KEY_VOLUMEUP |
| Home键 | 102 | KEY_HOME |
| 返回键 | 158 | KEY_BACK |
| 菜单键 | 139 | KEY_MENU |

### 查找输入设备

在Android设备上执行以下命令查找可用的输入设备：

```bash
# 列出所有输入设备
ls -la /dev/input/

# 查看设备信息
cat /proc/bus/input/devices

# 实时监听设备事件（测试用）
getevent /dev/input/event0
```

## 按键事件识别说明

### 事件类型定义

- **click (单击)**: 按键持续时间 0-500ms
- **double_click (双击)**: 两次单击间隔小于300ms
- **short_press (短按)**: 按键持续时间 500ms-1000ms  
- **long_press (长按)**: 按键持续时间大于2000ms
- **keydown/keyup**: 兼容模式，保持原有按下/抬起事件

### 时间阈值配置

所有时间参数都可以通过配置文件修改并即时生效：

```ini
click_threshold=500        # 单击阈值 (ms)
short_press_threshold=1000 # 短按阈值 (ms)
long_press_threshold=2000  # 长按阈值 (ms)
double_click_interval=300  # 双击间隔 (ms)
```

### 事件触发逻辑

1. 按键按下时启动计时
2. 按键释放时根据持续时间判断事件类型
3. 单击事件会等待双击间隔时间，确认是否为双击
4. 长按事件在达到阈值时立即触发
5. 所有事件都会传递事件类型参数给脚本

## 使用方法

### 1. 基本运行

```bash
# 连接到设备
adb shell

# 切换到程序目录
cd /data/adb/modules/kctrl

# 获取root权限
su

# 运行程序（使用默认配置）
./kctrl

# 使用自定义配置文件
./kctrl /path/to/custom/config.txt
```

### 1.1 按键检测工具 (kfind)

用于查找按键码，帮助配置 `config.txt` 文件：

```bash
# 运行按键检测
cd /data/adb/modules/kctrl
./kfind
```

- 按下你想要检测的按键，程序会显示按键码
- 使用 Ctrl+C 退出程序
- 将检测到的按键码添加到 `config.txt` 中

### 2. 后台运行

```bash
# 后台运行
nohup ./kctrl > /dev/null 2>&1 &

# 查看运行状态
ps | grep kctrl

# 停止程序
killall kctrl
```

### 3. 查看日志

```bash
# 查看系统日志
logcat | grep KCTRL

# 查看脚本执行日志
tail -f /data/adb/modules/kctrl/kctrl.log
```

## 脚本开发

脚本接收一个参数：事件类型（`keydown` 或 `keyup`）

### 脚本模板

```bash
#!/bin/bash

EVENT_TYPE=$1
LOG_FILE="/data/adb/modules/kctrl/kctrl.log"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

echo "[$TIMESTAMP] Key event: $EVENT_TYPE" >> $LOG_FILE

case $EVENT_TYPE in
    "keydown")
        # 按键按下时的操作
        echo "Key pressed" >> $LOG_FILE
        # 在这里添加你的代码
        ;;
    "keyup")
        # 按键抬起时的操作
        echo "Key released" >> $LOG_FILE
        # 在这里添加你的代码
        ;;
esac

exit 0
```

## 权限要求

程序需要以下权限：

1. **Root权限** - 访问系统级功能
2. **读取输入设备** - 访问 `/dev/input/eventX`
3. **WakeLock权限** - 访问 `/sys/power/wake_lock`
4. **文件系统权限** - 创建PID文件和日志文件

### 调试方法

```bash
# 查看程序日志
logcat | grep KCTRL

# 测试输入设备
getevent /dev/input/event0

# 检查进程状态
ps | grep kctrl

# 查看PID文件
cat ./mpid.txt
```

## 作者

**IDlike** - 项目作者和维护者

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。

## 免责声明

本工具仅供学习和研究使用，使用者需要自行承担使用风险。请确保在合法合规的前提下使用本工具。