@echo off
REM KCTRL Windows构建脚本
REM 使用Android NDK 28编译二进制文件

setlocal enabledelayedexpansion

REM 检查NDK路径
if "%ANDROID_NDK_ROOT%"=="" (
    echo Error: ANDROID_NDK_ROOT environment variable is not set
    echo Please set it to your Android NDK installation path
    echo Example: set ANDROID_NDK_ROOT=C:\android-ndk-r28
    pause
    exit /b 1
)

if not exist "%ANDROID_NDK_ROOT%" (
    echo Error: Android NDK not found at %ANDROID_NDK_ROOT%
    pause
    exit /b 1
)

echo Using Android NDK: %ANDROID_NDK_ROOT%

REM 创建构建目录
set BUILD_DIR=build
if exist "%BUILD_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM 设置目标架构（默认为arm64-v8a）
set TARGET_ARCH=%1
if "%TARGET_ARCH%"=="" set TARGET_ARCH=arm64-v8a
echo Building for architecture: %TARGET_ARCH%

REM 设置工具链
if "%TARGET_ARCH%"=="arm64-v8a" (
    set TOOLCHAIN_PREFIX=aarch64-linux-android
    set API_LEVEL=35
) else if "%TARGET_ARCH%"=="armeabi-v7a" (
    set TOOLCHAIN_PREFIX=armv7a-linux-androideabi
    set API_LEVEL=35
) else if "%TARGET_ARCH%"=="x86_64" (
    set TOOLCHAIN_PREFIX=x86_64-linux-android
    set API_LEVEL=35
) else if "%TARGET_ARCH%"=="x86" (
    set TOOLCHAIN_PREFIX=i686-linux-android
    set API_LEVEL=35
) else (
    echo Error: Unsupported architecture: %TARGET_ARCH%
    pause
    exit /b 1
)

REM 设置编译器路径
set TOOLCHAIN_DIR=%ANDROID_NDK_ROOT%\toolchains\llvm\prebuilt\windows-x86_64
set CXX=%TOOLCHAIN_DIR%\bin\%TOOLCHAIN_PREFIX%%API_LEVEL%-clang++.exe
set STRIP=%TOOLCHAIN_DIR%\bin\llvm-strip.exe

echo Using compiler: %CXX%

REM 检查编译器是否存在
if not exist "%CXX%" (
    echo Error: Compiler not found at %CXX%
    pause
    exit /b 1
)

REM 编译选项
set CXXFLAGS=-std=c++17 -Wall -Wextra -O2 -fPIE
set LDFLAGS=-pie -llog -landroid -lpthread

REM 编译kctrl
echo Compiling kctrl...
"%CXX%" %CXXFLAGS% -I"%ANDROID_NDK_ROOT%\sysroot\usr\include" -DANDROID_NDK -D__ANDROID_API__=%API_LEVEL% ..\main.cpp -o kctrl.exe %LDFLAGS%

if %ERRORLEVEL% equ 0 (
    echo kctrl build successful!
    
    REM 剥离符号以减小文件大小
    echo Stripping kctrl binary...
    "%STRIP%" kctrl.exe
    
    REM 编译kfind
    echo Compiling kfind...
    "%CXX%" %CXXFLAGS% -I"%ANDROID_NDK_ROOT%\sysroot\usr\include" -DANDROID_NDK -D__ANDROID_API__=%API_LEVEL% ..\kfind.cpp -o kfind.exe %LDFLAGS%
    
    if !ERRORLEVEL! equ 0 (
        echo kfind build successful!
        
        REM 剥离符号以减小文件大小
        echo Stripping kfind binary...
        "%STRIP%" kfind.exe
        
        REM 编译klaunch
        echo Compiling klaunch...
        "%CXX%" %CXXFLAGS% -I"%ANDROID_NDK_ROOT%\sysroot\usr\include" -DANDROID_NDK -D__ANDROID_API__=%API_LEVEL% ..\klaunch.cpp -o klaunch.exe %LDFLAGS%
        
        if !ERRORLEVEL! equ 0 (
            echo klaunch build successful!
            
            REM 剥离符号以减小文件大小
            echo Stripping klaunch binary...
            "%STRIP%" klaunch.exe
            
            REM 显示文件信息
            echo Binary info:
            dir kctrl.exe kfind.exe klaunch.exe
            
            echo.
            echo Binaries location: %CD%\
            echo To deploy to Android device:
            echo   adb push kctrl.exe /data/adb/modules/kctrl/kctrl
            echo   adb push kfind.exe /data/adb/modules/kctrl/kfind
            echo   adb push klaunch.exe /data/adb/modules/kctrl/klaunch
            echo   adb shell chmod 755 /data/adb/modules/kctrl/kctrl
            echo   adb shell chmod 755 /data/adb/modules/kctrl/kfind
            echo   adb shell chmod 755 /data/adb/modules/kctrl/klaunch
        ) else (
            echo klaunch build failed!
            pause
            exit /b 1
        )
    ) else (
        echo kfind build failed!
        pause
        exit /b 1
    )
) else (
    echo kctrl build failed!
    pause
    exit /b 1
)

pause