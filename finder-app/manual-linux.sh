#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
BUILD_OPS="ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE"

if [ $# -lt 1 ]
then
	echo "Using default directory $OUTDIR for output"
else
	OUTDIR=$1
	echo "Using passed directory $OUTDIR for output"
fi

mkdir -p $OUTDIR

cd "$OUTDIR"
if [ ! -d "$OUTDIR/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION $KERNEL_VERSION IN $OUTDIR"
	git clone $KERNEL_REPO --depth 1 --single-branch --branch $KERNEL_VERSION
fi
if [ ! -e $OUTDIR/linux-stable/arch/$ARCH/boot/Image ]; then
    cd linux-stable
    echo "Checking out version $KERNEL_VERSION"
    git checkout $KERNEL_VERSION

    # TODO: Add your kernel build steps here 
    echo $BUILD_OPS
    echo "mrproper" 
    make $BUILD_OPS mrproper
    echo "defconfig"
    make $BUILD_OPS CONFIG_BLK_DEV_INITRD=y defconfig
    echo "all"
    make -j $(nproc) $BUILD_OPS all
    echo "modules"
    make $BUILD_OPS modules
    echo "dtbs"
    make $BUILD_OPS dtbs
fi

echo "Adding the Image in outdir"
cp -v $OUTDIR/linux-stable/arch/$ARCH/boot/Image $OUTDIR

echo "Creating the staging directory for the root filesystem"
if [ -d "$OUTDIR/rootfs" ]
then
	echo "Deleting rootfs directory at $OUTDIR/rootfs and starting over"
    sudo rm  -rf $OUTDIR/rootfs
fi

# TODO: Create necessary base directories

mkdir $OUTDIR/rootfs/
cd $OUTDIR/rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin usr/lib64
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "$OUTDIR/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout $BUSYBOX_VERSION
    # TODO:  Configure busybox
    make distclean
    make $BUILD_OPS defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j $(nproc)  $BUILD_OPS CONFIG_PREFIX="$OUTDIR/rootfs" install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a $OUTDIR/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a $OUTDIR/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cd $OUTDIR/rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
LIBS64="libc.so.6 libc-2.31.so ld-2.31.so libm.so.6 libm-2.31.so libresolv.so.2 libresolv-2.31.so"
for lib in $LIBS64;
do
	cp -av $SYSROOT/lib64/$lib lib64
done

cp -av  $SYSROOT/lib/ld-linux-aarch64.so.1 lib


# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=$CROSS_COMPILE

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
SCRIPTS="writer finder.sh finder-test.sh conf autorun-qemu.sh"
for file in $SCRIPTS;
do
	cp -va $file $OUTDIR/rootfs/home
done
cp -vr ../conf $OUTDIR/rootfs/

# TODO: Chown the root directory
cd $OUTDIR/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip -f initramfs.cpio
