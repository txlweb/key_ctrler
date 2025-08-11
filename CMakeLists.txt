cmake_minimum_required(VERSION 3.18.1)

project("kctrl")

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# 添加可执行文件
add_executable(kctrl main.cpp)

# 链接Android库
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(kctrl
    ${log-lib}
    ${android-lib}
    -static-libstdc++
)

# 设置目标属性
set_target_properties(kctrl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 添加编译定义
target_compile_definitions(kctrl PRIVATE
    ANDROID_NDK
)

# 包含头文件目录
target_include_directories(kctrl PRIVATE
    ${ANDROID_NDK}/sysroot/usr/include
    ${ANDROID_NDK}/sysroot/usr/include/linux
)