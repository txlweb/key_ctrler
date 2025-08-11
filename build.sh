#!/bin/bash

# KCTRL 构建脚本
# 使用Android NDK 28编译二进制文件

set -e

# 检查NDK路径
if [ -z "$ANDROID_NDK_ROOT" ]; then
    echo "Error: ANDROID_NDK_ROOT environment variable is not set"
    echo "Please set it to your Android NDK installation path"
    echo "Example: export ANDROID_NDK_ROOT=/path/to/android-ndk-r28"
    exit 1
fi

if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $ANDROID_NDK_ROOT"
    exit 1
fi

echo "Using Android NDK: $ANDROID_NDK_ROOT"

# 创建构建目录
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 设置目标架构（默认为arm64-v8a）
TARGET_ARCH=${1:-arm64-v8a}
echo "Building for architecture: $TARGET_ARCH"

# 设置工具链
case $TARGET_ARCH in
    "arm64-v8a")
        TOOLCHAIN_PREFIX="aarch64-linux-android"
        API_LEVEL=35
        ;;
    "armeabi-v7a")
        TOOLCHAIN_PREFIX="armv7a-linux-androideabi"
        API_LEVEL=35
        ;;
    "x86_64")
        TOOLCHAIN_PREFIX="x86_64-linux-android"
        API_LEVEL=35
        ;;
    "x86")
        TOOLCHAIN_PREFIX="i686-linux-android"
        API_LEVEL=35
        ;;
    *)
        echo "Error: Unsupported architecture: $TARGET_ARCH"
        exit 1
        ;;
esac

# 设置编译器路径
TOOLCHAIN_DIR="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt"
if [ "$(uname)" == "Darwin" ]; then
    TOOLCHAIN_DIR="$TOOLCHAIN_DIR/darwin-x86_64"
elif [ "$(uname)" == "Linux" ]; then
    TOOLCHAIN_DIR="$TOOLCHAIN_DIR/linux-x86_64"
else
    TOOLCHAIN_DIR="$TOOLCHAIN_DIR/windows-x86_64"
fi

CC="$TOOLCHAIN_DIR/bin/${TOOLCHAIN_PREFIX}${API_LEVEL}-clang"
CXX="$TOOLCHAIN_DIR/bin/${TOOLCHAIN_PREFIX}${API_LEVEL}-clang++"
STRIP="$TOOLCHAIN_DIR/bin/llvm-strip"

echo "Using compiler: $CXX"

# 检查编译器是否存在
if [ ! -f "$CXX" ]; then
    echo "Error: Compiler not found at $CXX"
    exit 1
fi

# 编译选项
CFLAGS="-Wall -Wextra -O2 -fPIE"
CXXFLAGS="-std=c++17 -Wall -Wextra -O2 -fPIE"
LDFLAGS="-pie -llog -landroid -static-libstdc++"

# 编译kctrl
echo "Compiling kctrl..."
"$CXX" $CXXFLAGS -I"$ANDROID_NDK_ROOT/sysroot/usr/include" \
    -DANDROID_NDK -D__ANDROID_API__=$API_LEVEL \
    ../main.cpp -o kctrl $LDFLAGS

if [ $? -eq 0 ]; then
    echo "kctrl build successful!"
    
    # 剥离符号以减小文件大小
    echo "Stripping kctrl binary..."
    "$STRIP" kctrl
    
    # 编译kfind
    echo "Compiling kfind..."
    "$CXX" $CXXFLAGS -I"$ANDROID_NDK_ROOT/sysroot/usr/include" \
        -DANDROID_NDK -D__ANDROID_API__=$API_LEVEL \
        ../kfind.cpp -o kfind $LDFLAGS
    
    if [ $? -eq 0 ]; then
        echo "kfind build successful!"
        
        # 剥离符号以减小文件大小
        echo "Stripping kfind binary..."
        "$STRIP" kfind
        
        # 编译klaunch
        echo "Compiling klaunch..."
        "$CXX" $CXXFLAGS -I"$ANDROID_NDK_ROOT/sysroot/usr/include" \
            -DANDROID_NDK -D__ANDROID_API__=$API_LEVEL \
            ../klaunch.cpp -o klaunch $LDFLAGS
        
        if [ $? -eq 0 ]; then
            echo "klaunch build successful!"
            
            # 剥离符号以减小文件大小
            echo "Stripping klaunch binary..."
            "$STRIP" klaunch
            
            # 显示文件信息
            echo "Binary info:"
            ls -lh kctrl kfind klaunch
            file kctrl kfind klaunch
            
            echo ""
            echo "Binaries location: $(pwd)/"
            echo "To deploy to Android device:"
            echo "  adb push kctrl /data/adb/modules/kctrl/"
            echo "  adb push kfind /data/adb/modules/kctrl/"
            echo "  adb push klaunch /data/adb/modules/kctrl/"
            echo "  adb shell chmod 755 /data/adb/modules/kctrl/kctrl"
            echo "  adb shell chmod 755 /data/adb/modules/kctrl/kfind"
            echo "  adb shell chmod 755 /data/adb/modules/kctrl/klaunch"
        else
            echo "klaunch build failed!"
            exit 1
        fi
    else
        echo "kfind build failed!"
        exit 1
    fi
else
    echo "kctrl build failed!"
    exit 1
fi