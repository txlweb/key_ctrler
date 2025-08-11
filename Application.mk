# 设置目标平台 (Android 10+)
APP_PLATFORM := android-29

# 设置目标架构
APP_ABI := arm64-v8a armeabi-v7a x86_64 x86

# 设置STL
APP_STL := c++_shared

# 设置优化级别
APP_OPTIM := release

# 启用异常处理
APP_CPPFLAGS := -fexceptions -frtti

# 设置最小日志级别
APP_CFLAGS := -DANDROID_LOG_LEVEL=ANDROID_LOG_INFO