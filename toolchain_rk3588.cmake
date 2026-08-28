# CMake toolchain file for RK3588 cross-compilation
# 用于交叉编译目标为 aarch64 Linux 系统

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 工具链安装目录（如有变化请修改此处）
set(TOOLCHAIN_DIR /opt/atk-dlrk3588-toolchain)

# 交叉编译器
set(CMAKE_C_COMPILER   ${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-g++)
set(CMAKE_AR           ${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-ar)
set(CMAKE_LINKER       ${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-ld)
set(CMAKE_STRIP        ${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-strip)

# 工具链自带的 sysroot（提供基础 C/C++ 库）
set(TOOLCHAIN_SYSROOT ${TOOLCHAIN_DIR}/aarch64-buildroot-linux-gnu/sysroot)

# 设置 CMAKE_SYSROOT 为工具链 sysroot
set(CMAKE_SYSROOT ${TOOLCHAIN_SYSROOT})

# 设置库架构，使 find_library 能自动搜索 lib/aarch64-linux-gnu 等多架构目录
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

# 重要：明确查找路径策略，避免程序（如 Python 解释器）被重定向到 sysroot 中
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # 程序只在宿主机查找
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # 库只在 sysroot/交叉根路径中查找
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # 头文件只在 sysroot/交叉根路径中查找
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)    # 包只在 sysroot/交叉根路径中查找

# 编译选项（按需调整）
add_compile_options(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)
add_compile_options(-Wno-write-strings -Wno-return-type)