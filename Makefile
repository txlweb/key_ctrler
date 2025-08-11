# KCTRL Makefile
# 使用Android NDK编译

# 默认目标架构
TARGET_ARCH ?= arm64-v8a
API_LEVEL = 35

# 检查NDK路径
ifeq ($(ANDROID_NDK_ROOT),)
    $(error ANDROID_NDK_ROOT is not set. Please set it to your Android NDK installation path)
endif

# 根据架构设置工具链
ifeq ($(TARGET_ARCH),arm64-v8a)
    TOOLCHAIN_PREFIX = aarch64-linux-android
else ifeq ($(TARGET_ARCH),armeabi-v7a)
    TOOLCHAIN_PREFIX = armv7a-linux-androideabi
else ifeq ($(TARGET_ARCH),x86_64)
    TOOLCHAIN_PREFIX = x86_64-linux-android
else ifeq ($(TARGET_ARCH),x86)
    TOOLCHAIN_PREFIX = i686-linux-android
else
    $(error Unsupported architecture: $(TARGET_ARCH))
endif

# 检测操作系统
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    HOST_TAG = linux-x86_64
else ifeq ($(UNAME_S),Darwin)
    HOST_TAG = darwin-x86_64
else
    HOST_TAG = windows-x86_64
endif

# 工具链路径
TOOLCHAIN_DIR = $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(HOST_TAG)
CC = $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_PREFIX)$(API_LEVEL)-clang
CXX = $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_PREFIX)$(API_LEVEL)-clang++
STRIP = $(TOOLCHAIN_DIR)/bin/llvm-strip

# 编译选项 - 极致内存优化版本
CFLAGS = -Wall -Wextra -Oz -fPIE -ffunction-sections -fdata-sections -flto -fvisibility=hidden -fomit-frame-pointer
CXXFLAGS = -std=c++17 -Wall -Wextra -Oz -fPIE -ffunction-sections -fdata-sections -flto -fno-rtti -fno-exceptions -fvisibility=hidden -fomit-frame-pointer
CPPFLAGS = -I$(ANDROID_NDK_ROOT)/sysroot/usr/include -DANDROID_NDK -D__ANDROID_API__=$(API_LEVEL)
LDFLAGS = -pie -llog -landroid -static-libstdc++ -Wl,--gc-sections -Wl,--strip-all -Wl,--strip-debug -Wl,--discard-all -flto -s

# 源文件和目标文件
SRCS = main.cpp
KFIND_SRCS = kfind.cpp
KLAUNCH_SRCS = klaunch.cpp
TARGET = kctrl
KFIND_TARGET = kfind
KLAUNCH_TARGET = klaunch
BUILD_DIR = build

# 默认目标
all: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(KFIND_TARGET) $(BUILD_DIR)/$(KLAUNCH_TARGET)


$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 编译kctrl目标
$(BUILD_DIR)/$(TARGET): $(SRCS) | $(BUILD_DIR)
	@echo "Building $(TARGET) for $(TARGET_ARCH)..."
	@echo "Using compiler: $(CXX)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Stripping $(TARGET) binary..."
	$(STRIP) $@
	@echo "Build completed: $@"

# 编译kfind目标
$(BUILD_DIR)/$(KFIND_TARGET): $(KFIND_SRCS) | $(BUILD_DIR)
	@echo "Building $(KFIND_TARGET) for $(TARGET_ARCH)..."
	@echo "Using compiler: $(CXX)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Stripping $(KFIND_TARGET) binary..."
	$(STRIP) $@
	@echo "Build completed: $@"

# 编译klaunch目标
$(BUILD_DIR)/$(KLAUNCH_TARGET): $(KLAUNCH_SRCS) | $(BUILD_DIR)
	@echo "Building $(KLAUNCH_TARGET) for $(TARGET_ARCH)..."
	@echo "Using compiler: $(CXX)"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Stripping $(KLAUNCH_TARGET) binary..."
	$(STRIP) $@
	@echo "Build completed: $@"
	@ls -lh $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(KFIND_TARGET) $(BUILD_DIR)/$(KLAUNCH_TARGET)

# 清理
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

# 部署到设备
deploy: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(KFIND_TARGET) $(BUILD_DIR)/$(KLAUNCH_TARGET)
	@echo "Deploying to Android device..."
	adb shell "mkdir -p /data/adb/modules/kctrl"
	adb shell "mkdir -p /data/adb/modules/kctrl/scripts"
	adb push $(BUILD_DIR)/$(TARGET) /data/adb/modules/kctrl/
	adb push $(BUILD_DIR)/$(KFIND_TARGET) /data/adb/modules/kctrl/
	adb push $(BUILD_DIR)/$(KLAUNCH_TARGET) /data/adb/modules/kctrl/
	adb push config.txt /data/adb/modules/kctrl/
	adb push scripts/ /data/adb/modules/kctrl/
	adb shell "chmod 755 /data/adb/modules/kctrl/$(TARGET)"
	adb shell "chmod 755 /data/adb/modules/kctrl/$(KFIND_TARGET)"
	adb shell "chmod 755 /data/adb/modules/kctrl/$(KLAUNCH_TARGET)"
	adb shell "chmod 755 /data/adb/modules/kctrl/scripts/*.sh"
	@echo "Deployment completed!"

# 运行（需要设备连接）
run: deploy
	@echo "Starting KCTRL on device..."
	adb shell "cd /data/adb/modules/kctrl && su -c './$(TARGET)'"

# 停止运行
stop:
	@echo "Stopping KCTRL..."
	adb shell "su -c 'killall $(TARGET)'"

# 查看日志
log:
	@echo "Showing KCTRL logs..."
	adb logcat | grep KCTRL

# 帮助信息
help:
	@echo "KCTRL Makefile Usage:"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build the binary (default)"
	@echo "  clean      - Clean build directory"
	@echo "  deploy     - Deploy to Android device"
	@echo "  run        - Deploy and run on device"
	@echo "  stop       - Stop running instance"
	@echo "  log        - Show application logs"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  TARGET_ARCH - Target architecture (arm64-v8a, armeabi-v7a, x86_64, x86)"
	@echo ""
	@echo "Examples:"
	@echo "  make                              # Build for arm64-v8a"
	@echo "  make TARGET_ARCH=armeabi-v7a      # Build for armeabi-v7a"
	@echo "  make deploy                       # Deploy to device"
	@echo "  make run                          # Build, deploy and run"

# 声明伪目标
.PHONY: all clean deploy run stop log help