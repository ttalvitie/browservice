#!/bin/bash

set -e
set -o pipefail

msg() { cat <<< "$@" 1>&2; }

if [ -z "${1}" ] || [ -z "${2}" ] || ! [ -z "${3}" ]
then
        msg "Invalid arguments"
        msg "Usage: build_appimage.sh x86_64|i386|armhf|aarch64 BRANCH|COMMIT|TAG"
        exit 1
fi

ARCH="${1}"
SRC="${2}"

if [ "${ARCH}" == "x86_64" ]
then
    UBUNTU_ARCH="amd64"
    UBUNTU_KERNEL="vmlinuz-generic"
    UBUNTU_INITRD="initrd-generic"
    QEMU="qemu-system-x86_64"
    QEMU_ARCH_OPTS=""
    QEMU_DISK_DEV="virtio-blk-pci"
    QEMU_9P_DEV="virtio-9p-pci"
elif [ "${ARCH}" == "i386" ]
then
    UBUNTU_ARCH="i386"
    UBUNTU_KERNEL="vmlinuz-generic"
    UBUNTU_INITRD="initrd-generic"
    QEMU="qemu-system-i386"
    QEMU_ARCH_OPTS=""
    QEMU_DISK_DEV="virtio-blk-pci"
    QEMU_9P_DEV="virtio-9p-pci"
elif [ "${ARCH}" == "armhf" ]
then
    UBUNTU_ARCH="armhf"
    UBUNTU_KERNEL="vmlinuz-lpae"
    UBUNTU_INITRD="initrd-generic-lpae"
    QEMU="qemu-system-arm"
    QEMU_ARCH_OPTS="-machine virt"
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
elif [ "${ARCH}" == "aarch64" ]
then
    UBUNTU_ARCH="arm64"
    UBUNTU_KERNEL="vmlinuz-generic"
    UBUNTU_INITRD="initrd-generic"
    QEMU="qemu-system-aarch64"
    QEMU_ARCH_OPTS="-machine virt -cpu cortex-a57"
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
else
    msg "ERROR: unsupported architecture '${ARCH}'"
    exit 1
fi

if [ "${UID}" -eq "0" ]
then
    msg "ERROR: running as root"
    exit 1
fi

if [ "$(git rev-parse --is-inside-work-tree)" != "true" ]
then
    msg "ERROR: cwd is not a git work tree"
    exit 1
fi

if ! "${QEMU}" --version > /dev/null 2> /dev/null
then
    msg "ERROR: running '${QEMU} --version' resulted in nonzero exit status"
    exit 1
fi

if ! command -v cloud-localds > /dev/null 2> /dev/null
then
    msg "ERROR: command 'cloud-localds' does not exist"
    exit 1
fi

TMPDIR="$(mktemp -d)"

onexit() {
    msg "Building AppImage failed"
    rm -rf -- "${TMPDIR}" > /dev/null 2>&1 || true
}
trap onexit EXIT

mkdir "${TMPDIR}/shared"
mkdir "${TMPDIR}/vm"

msg "Generating tarball from branch/commit/tag '${SRC}'"
pushd "$(git rev-parse --show-toplevel)" > /dev/null
git archive --output="${TMPDIR}/shared/src.tar" "${SRC}"
popd > /dev/null

msg "Downloading Ubuntu Bionic Cloud VM image"
wget "https://cloud-images.ubuntu.com/bionic/current/bionic-server-cloudimg-${UBUNTU_ARCH}.img" -O "${TMPDIR}/vm/disk.img"
wget "https://cloud-images.ubuntu.com/bionic/current/unpacked/bionic-server-cloudimg-${UBUNTU_ARCH}-${UBUNTU_KERNEL}" -O "${TMPDIR}/vm/kernel"
wget "https://cloud-images.ubuntu.com/bionic/current/unpacked/bionic-server-cloudimg-${UBUNTU_ARCH}-${UBUNTU_INITRD}" -O "${TMPDIR}/vm/initrd"

msg "Creating cloud-init user data image"
cat << EOF > "${TMPDIR}/vm/user-data"
#!/bin/bash
mkdir /shared
mount -t 9p -o trans=virtio shared /shared -oversion=9p2000.L
echo SUCCESS > /shared/result
poweroff
EOF
cloud-localds "${TMPDIR}/vm/user-data.img" "${TMPDIR}/vm/user-data"

msg "Enlarging VM disk image"
qemu-img resize "${TMPDIR}/vm/disk.img" +20G

msg "Starting Ubuntu in QEMU"
"${QEMU}" \
    -m 2048 \
    -smp 2 \
    ${QEMU_ARCH_OPTS} \
    -drive "file=${TMPDIR}/vm/disk.img,format=qcow2,if=none,id=hd0" \
    -drive "file=${TMPDIR}/vm/user-data.img,format=raw,if=none,id=hd1" \
    -device "${QEMU_DISK_DEV},drive=hd0,serial=hd0" \
    -device "${QEMU_DISK_DEV},drive=hd1,serial=hd1" \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    -kernel "${TMPDIR}/vm/kernel" \
    -initrd "${TMPDIR}/vm/initrd" \
    -append "rw root=/dev/disk/by-id/virtio-hd0-part1" \
    -fsdev "local,path=${TMPDIR}/shared,security_model=mapped-xattr,id=shared,writeout=immediate,fmode=0644,dmode=0755" \
    -device "${QEMU_9P_DEV},fsdev=shared,mount_tag=shared" \
    -display none

cat "${TMPDIR}/shared/result"

trap - EXIT
rm -rf -- "${TMPDIR}" > /dev/null 2>&1
msg "Success"
