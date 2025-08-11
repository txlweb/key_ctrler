LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := kctrl
LOCAL_SRC_FILES := main.cpp

# 设置C++标准和极致内存优化标志
LOCAL_CPPFLAGS := -std=c++17 -Wall -Wextra -Oz -ffunction-sections -fdata-sections -flto -fno-rtti -fno-exceptions -fvisibility=hidden -fomit-frame-pointer
LOCAL_CFLAGS := -Wall -Wextra -Oz -ffunction-sections -fdata-sections -flto -fvisibility=hidden -fomit-frame-pointer

# 链接库和极致优化标志
LOCAL_LDLIBS := -llog -landroid -static-libstdc++
LOCAL_LDFLAGS := -Wl,--gc-sections -Wl,--strip-all -Wl,--strip-debug -Wl,--discard-all -flto -s

# 设置API级别
LOCAL_CPPFLAGS += -DANDROID_NDK

# 设置目标架构
LOCAL_ARM_MODE := arm

include $(BUILD_EXECUTABLE)

# KLAUNCH模块
include $(CLEAR_VARS)
LOCAL_MODULE := klaunch
LOCAL_SRC_FILES := klaunch.cpp
LOCAL_CPPFLAGS := -std=c++11 -Wall -Wextra
LOCAL_LDLIBS := -llog
include $(BUILD_EXECUTABLE)
