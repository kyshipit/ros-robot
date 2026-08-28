#!/bin/bash
#
# build-linux.sh — colcon 交叉编译 eai_bot 包（RK3588 / aarch64 / ROS2 Jazzy）
#
# 用法：
#   cd <workspace_root>
#   ./build-linux.sh
#
# 依赖：
#   1. 交叉工具链：/opt/atk-dlrk3588-toolchain
#   2. 板端 ROS2 sysroot：默认 ~/software/rk_sysroot（可通过 SYSROOT_ROS 环境变量覆盖）
#   3. 宿主机安装 colcon 及相关工具

set -e

ROOT_PWD=$(cd "$(dirname "$0")" && pwd)
SYSROOT_ROS="${SYSROOT_ROS:-${HOME}/software/rk_sysroot}"
TOOLCHAIN_DIR="/opt/atk-dlrk3588-toolchain"

# 宿主机 Python3 解释器路径
HOST_PYTHON3="$(which python3)"
if [[ -z "${HOST_PYTHON3}" ]]; then
  echo "ERROR: python3 not found in host environment"
  exit 1
fi

echo "==================================="
echo "Workspace: ${ROOT_PWD}"
echo "ROS sysroot: ${SYSROOT_ROS}"
echo "Toolchain: ${TOOLCHAIN_DIR}"
echo "Host Python3: ${HOST_PYTHON3}"
echo "==================================="

# 检查必要目录
if [[ ! -d "${SYSROOT_ROS}/opt/ros/jazzy" ]]; then
  echo "ERROR: ROS2 sysroot not found at ${SYSROOT_ROS}/opt/ros/jazzy"
  exit 1
fi

if [[ ! -f "${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-g++" ]]; then
  echo "ERROR: RK3588 cross-compiler not found"
  exit 1
fi

cd "${ROOT_PWD}"

# 构建参数
# 关键：-DCMAKE_SYSROOT 设置为 ROS sysroot，解决绝对路径依赖问题
colcon build \
  --packages-select eai_bot \
  --cmake-args \
    -DCMAKE_TOOLCHAIN_FILE="${ROOT_PWD}/toolchain_rk3588.cmake" \
    -DCMAKE_SYSROOT="${SYSROOT_ROS}" \
    -DCMAKE_FIND_ROOT_PATH="${SYSROOT_ROS};${TOOLCHAIN_DIR}/aarch64-buildroot-linux-gnu/sysroot" \
    -DROS_SYSROOT="${SYSROOT_ROS}" \
    -DPython3_EXECUTABLE="${HOST_PYTHON3}" \
    -DTARGET_SOC=rk3588 \
    -DCMAKE_BUILD_TYPE=Release

echo ""
echo "==================================="
echo "Build complete."
echo "Install dir: ${ROOT_PWD}/install"
echo ""
echo "To deploy to RK3588 board:"
echo "  rsync -avz install/ root@<board_ip>:/path/to/workspace/install/"
echo "==================================="