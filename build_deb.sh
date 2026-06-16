#!/bin/bash
set -e

# ============ 打包 ktouch deb ============
# 用法: ./build_deb.sh [输出目录]
# 输出: ktouch_<版本>_<架构>.deb

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${1:-$(pwd)}"
BUILD_DIR="/tmp/ktouch_deb_build"

# 从 control 读取版本和架构
CONTROL_FILE="$SCRIPT_DIR/DEBIAN/control"
if [ ! -f "$CONTROL_FILE" ]; then
    echo "错误: 找不到 $CONTROL_FILE"
    exit 1
fi

VERSION=$(grep -i "^Version:" "$CONTROL_FILE" | awk '{print $2}')
ARCH=$(grep -i "^Architecture:" "$CONTROL_FILE" | awk '{print $2}')
PACKAGE=$(grep -i "^Package:" "$CONTROL_FILE" | awk '{print $2}')
DEB_FILE="${PACKAGE}_${VERSION}_${ARCH}.deb"

echo "============================================"
echo "  打包 $PACKAGE v$VERSION ($ARCH)"
echo "============================================"

# 1. 清理旧构建目录
if [ -d "$BUILD_DIR" ]; then
    echo "[1/5] 清理旧构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 2. 创建目录结构并拷贝文件
echo "[2/5] 创建构建目录并拷贝文件..."
mkdir -p "$BUILD_DIR/DEBIAN"
mkdir -p "$BUILD_DIR/opt"
mkdir -p "$BUILD_DIR/usr/share/doc/ktouch"

cp -r "$SCRIPT_DIR/DEBIAN/"*  "$BUILD_DIR/DEBIAN/"
cp -r "$SCRIPT_DIR/opt/ktouch" "$BUILD_DIR/opt/"

[ -f "$SCRIPT_DIR/changelog" ] && cp "$SCRIPT_DIR/changelog" "$BUILD_DIR/usr/share/doc/ktouch/changelog"
[ -f "$SCRIPT_DIR/LICENSE"   ] && cp "$SCRIPT_DIR/LICENSE"   "$BUILD_DIR/usr/share/doc/ktouch/copyright"

# 3. 设置权限
echo "[3/5] 设置文件权限..."

chmod 755 "$BUILD_DIR/DEBIAN"
chmod 644 "$BUILD_DIR/DEBIAN/control"

for script in preinst postinst prerm postrm; do
    [ -f "$BUILD_DIR/DEBIAN/$script" ] && chmod 755 "$BUILD_DIR/DEBIAN/$script"
done

# # opt 下所有文件默认 644，目录 755
# find "$BUILD_DIR/opt" -type d -exec chmod 755 {} \;
# find "$BUILD_DIR/opt" -type f -exec chmod 644 {} \;

# # 给可执行程序设置 755
# EXE_LIST=(
#     check_save
#     screen_binder
#     screen_ds
#     screen_ds_once
#     touch_ds
#     touch_set
#     usb_ds
#     kdisplay
#     kscreen-remap
#     kscreen-remap-daemon
#     kscreen-setup
#     kscreen-debug
#     kscreen-log
#     kscreen-fix-daemon
#     display_monitor.py
# )

# for exe in "${EXE_LIST[@]}"; do
#     [ -f "$BUILD_DIR/opt/ktouch/$exe" ] && chmod 755 "$BUILD_DIR/opt/ktouch/$exe"
# done

# usr 下文档 644，目录 755
find "$BUILD_DIR/usr" -type d -exec chmod 755 {} \;
find "$BUILD_DIR/usr" -type f -exec chmod 644 {} \;

# 4. dpkg 打包
echo "[4/5] 执行 dpkg-deb 打包..."
dpkg-deb -b "$BUILD_DIR" "$DEB_FILE"

# 5. 清理
echo "[5/5] 清理临时目录..."
rm -rf "$BUILD_DIR"

# 移动到输出目录
if [ "$(realpath "$OUTPUT_DIR")" != "$(realpath "$(pwd)")" ]; then
    mv "$DEB_FILE" "$OUTPUT_DIR/"
fi

echo ""
echo "============================================"
echo "  打包完成: $OUTPUT_DIR/$DEB_FILE"
echo "  大小: $(du -h "$OUTPUT_DIR/$DEB_FILE" | cut -f1)"
echo "============================================"
