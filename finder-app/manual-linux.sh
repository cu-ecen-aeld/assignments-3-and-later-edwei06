#!/bin/bash
# Script to fetch, build an aarch64 Linux kernel, BusyBox, and assemble an initramfs
# Author: Siddhant Jajoo  -- updated/expanded by ChatGPT

set -e          # exit on first error
set -u          # treat unset vars as error

###############################################################################
# --------- 參數與環境 ---------------------------------------------------------
###############################################################################
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath "$(dirname "$0")")
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# 允許以 ./script.sh <out-directory> 指定輸出目錄
if [ $# -ge 1 ]; then
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
else
    echo "Using default directory ${OUTDIR} for output"
fi
mkdir -p "${OUTDIR}"

###############################################################################
# --------- 下載並編譯 Linux kernel ------------------------------------------
###############################################################################
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone "${KERNEL_REPO}" --depth 1 --single-branch --branch "${KERNEL_VERSION}" linux-stable
fi

if [ ! -e "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout "${KERNEL_VERSION}"

    echo "Cleaning previous builds (if any)"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    echo "Configuring kernel (defconfig for virt-aarch64)"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    echo "Building kernel Image, modules, and DTBs – this may take a while…"
    make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}/"

###############################################################################
# --------- 建立 rootfs 目錄結構 ----------------------------------------------
###############################################################################
echo "Creating the staging directory for the root filesystem"
cd "${OUTDIR}"
if [ -d "${OUTDIR}/rootfs" ]; then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf "${OUTDIR}/rootfs"
fi

# 建立基本目錄階層
mkdir -p rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var}
mkdir -p rootfs/usr/{bin,sbin,lib}
mkdir -p rootfs/var/log
chmod 1777 rootfs/tmp        # tmp 需為 world-writable, sticky

###############################################################################
# --------- 下載並編譯 BusyBox -------------------------------------------------
###############################################################################
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]; then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout "${BUSYBOX_VERSION}"

    echo "Configuring busybox (static build)"
    make distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # 啟用 static 連結，確保 initramfs 開機就能執行
    sed -i 's/.*CONFIG_STATIC.*/CONFIG_STATIC=y/' .config
else
    cd busybox
fi

echo "Building and installing BusyBox"
make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX="${OUTDIR}/rootfs" install

###############################################################################
# --------- 複製所需共享函式庫 -------------------------------------------------
###############################################################################
echo "Resolving library dependencies"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

# 解析 busybox 需要的動態載入器與 libs（若為 static build，下面仍示範流程）
NEEDED_LIBS=$(\
    ${CROSS_COMPILE}readelf -a "${OUTDIR}/rootfs/bin/busybox" | \
    grep "Shared library" | awk '{print $NF}' | tr -d '[]')

for LIB in $NEEDED_LIBS; do
    # 在 sysroot 中尋找並複製
    SRC=$(find "${SYSROOT}" -name "${LIB}" | head -n 1 || true)
    if [ -n "${SRC}" ]; then
        DST_DIR="${OUTDIR}/rootfs/$(dirname "${SRC#"${SYSROOT}/"}")"
        mkdir -p "${DST_DIR}"
        cp -a "${SRC}" "${DST_DIR}/"
    fi
done

# 動態載入器 (ld-linux-aarch64.so.1)
LDSO=$(find "${SYSROOT}" -name "ld-linux-aarch64.so.1" | head -n 1 || true)
if [ -n "${LDSO}" ]; then
    mkdir -p "${OUTDIR}/rootfs/$(dirname "${LDSO#"${SYSROOT}/"}")"
    cp -a "${LDSO}" "${OUTDIR}/rootfs/$(dirname "${LDSO#"${SYSROOT}/"}")/"
fi

###############################################################################
# --------- 建立裝置節點 -------------------------------------------------------
###############################################################################
echo "Creating dev nodes"
cd "${OUTDIR}/rootfs"  
sudo mknod -m 666 "${OUTDIR}/rootfs/dev/null"    c 1 3
sudo mknod -m 622 "${OUTDIR}/rootfs/dev/console" c 5 1

###############################################################################
# --------- 編譯 writer 實用程式 ----------------------------------------------
###############################################################################
echo "Building writer utility"
cd "${FINDER_APP_DIR}"

# 有 clean 就執行；沒有就靜默略過
make -q clean >/dev/null 2>&1 && make clean || echo "No clean target, skipping …"

# 交叉編譯 writer
make CC=${CROSS_COMPILE}gcc

###############################################################################
# --------- 複製 finder 腳本 / 執行檔 -----------------------------------------
###############################################################################
echo "Copying finder scripts and executables to target rootfs"
mkdir -p "${OUTDIR}/rootfs/home"
cp -a "${FINDER_APP_DIR}"/{finder.sh,finder-test.sh,autorun-qemu.sh} "${OUTDIR}/rootfs/home/"
cp -a "${FINDER_APP_DIR}/writer" "${OUTDIR}/rootfs/home/"

# 若有其他自定義 conf 或 txt 檔案，可一併複製
if [ -d "${FINDER_APP_DIR}/conf" ]; then
    cp -a "${FINDER_APP_DIR}/conf" "${OUTDIR}/rootfs/home/"
fi

###############################################################################
# --------- 調整擁有權 ---------------------------------------------------------
###############################################################################
echo "Setting ownership to root:root"
sudo chown -R root:root "${OUTDIR}/rootfs"

###############################################################################
# --------- 產生 initramfs -----------------------------------------------------
###############################################################################
echo "Creating initramfs.cpio.gz"
cd "${OUTDIR}/rootfs"
find . | cpio --owner root:root -H newc -ov > "${OUTDIR}/initramfs.cpio"
cd "${OUTDIR}"
gzip -f initramfs.cpio

echo "All done!"
echo "Kernel Image: ${OUTDIR}/Image"
echo "Initramfs:    ${OUTDIR}/initramfs.cpio.gz"

