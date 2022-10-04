#!/bin/bash

set -e
set -o pipefail

msg() { cat <<< "--- $@" 1>&2; }

if [ -z "${1}" ] || [ -z "${2}" ] || [ -z "${3}" ] || [ -z "${4}" ] || ! [ -z "${5}" ]
then
    msg "Invalid arguments"
    msg "Usage: build_appimage.sh x86_64|armhf|aarch64 BRANCH|COMMIT|TAG PATCHED_CEF_TARBALL OUTPUT"
    exit 1
fi

ARCH="${1}"
SRC="${2}"
CEFTARBALL="${3}"
OUTPUT="${4}"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

if ! [ -f "${CEFTARBALL}" ]
then
    msg "ERROR: Given CEF tarball path '${CEFTARBALL}' is not a file"
    exit 1
fi

if [ "${ARCH}" == "x86_64" ]
then
    UBUNTU_ARCH="amd64"
    UBUNTU_RELEASE="focal"
    UBUNTU_KERNEL="vmlinuz-generic"
    UBUNTU_INITRD="initrd-generic"
    QEMU="qemu-system-x86_64"
    QEMU_ARCH_OPTS=""
    QEMU_DISK_DEV="virtio-blk-pci"
    QEMU_9P_DEV="virtio-9p-pci"
    QEMU_NET_DEV="virtio-net-pci"
elif [ "${ARCH}" == "armhf" ]
then
    UBUNTU_ARCH="armhf"
    UBUNTU_RELEASE="focal"
    UBUNTU_KERNEL="vmlinuz-lpae"
    UBUNTU_INITRD="initrd-generic-lpae"
    QEMU="qemu-system-arm"
    QEMU_ARCH_OPTS="-machine virt"
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
    QEMU_NET_DEV="virtio-net-device"
elif [ "${ARCH}" == "aarch64" ]
then
    UBUNTU_ARCH="arm64"
    UBUNTU_RELEASE="focal"
    UBUNTU_KERNEL="vmlinuz-generic"
    UBUNTU_INITRD="initrd-generic"
    QEMU="qemu-system-aarch64"
    QEMU_ARCH_OPTS="-machine virt -cpu cortex-a57"
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
    QEMU_NET_DEV="virtio-net-device"
else
    msg "ERROR: unsupported architecture '${ARCH}'"
    exit 1
fi

if [ "${UID}" -eq "0" ]
then
    msg "ERROR: running as root"
    exit 1
fi

pushd "${SCRIPT_DIR}" &> /dev/null
if [ "$(git rev-parse --is-inside-work-tree)" != "true" ]
then
    msg "ERROR: script is not inside a git work tree"
    exit 1
fi
popd &> /dev/null

if ! "${QEMU}" --version &> /dev/null
then
    msg "ERROR: running '${QEMU} --version' resulted in nonzero exit status"
    exit 1
fi

if ! command -v cloud-localds &> /dev/null
then
    msg "ERROR: command 'cloud-localds' does not exist"
    exit 1
fi

TMPDIR="$(mktemp -d)"
QEMUPID=""
TAILPID=""

onexit() {
    msg "Building AppImage failed"
    rm -rf -- "${TMPDIR}" &> /dev/null || true
    if ! [ -z "${QEMUPID}" ]
    then
        kill "${QEMUPID}" &> /dev/null || true
    fi
    if ! [ -z "${TAILPID}" ]
    then
        kill "${TAILPID}" &> /dev/null || true
    fi
}
trap onexit EXIT

NAME="browservice-${SRC//\//-}-${ARCH}.AppImage"
msg "Building ${NAME}"

msg "Populating shared directory for QEMU machine"
mkdir "${TMPDIR}/shared"
touch "${TMPDIR}/shared/log"
cp "${SCRIPT_DIR}/appimage_build_data/build_appimage_impl.sh" "${TMPDIR}/shared"
cp "${SCRIPT_DIR}/appimage_build_data/browservice.png" "${TMPDIR}/shared/browservice.png"
cp "${SCRIPT_DIR}/appimage_build_data/browservice.desktop" "${TMPDIR}/shared/browservice.desktop"
cp "${SCRIPT_DIR}/appimage_build_data/run_browservice" "${TMPDIR}/shared/run_browservice"
cp "${SCRIPT_DIR}/appimage_build_data/relocate_fontconfig_cache.c" "${TMPDIR}/shared/relocate_fontconfig_cache.c"
cp "${CEFTARBALL}" "${TMPDIR}/shared/cef.tar.bz2"

msg "Generating tarball from branch/commit/tag '${SRC}'"
pushd "${SCRIPT_DIR}" &> /dev/null
pushd "$(git rev-parse --show-toplevel)" &> /dev/null
git archive --output="${TMPDIR}/shared/src.tar" "${SRC}"
popd &> /dev/null
popd &> /dev/null

msg "Downloading Ubuntu ${UBUNTU_RELEASE} Cloud VM image"
mkdir "${TMPDIR}/vm"
wget "https://cloud-images.ubuntu.com/${UBUNTU_RELEASE}/current/${UBUNTU_RELEASE}-server-cloudimg-${UBUNTU_ARCH}.img" -O "${TMPDIR}/vm/disk.img"
wget "https://cloud-images.ubuntu.com/${UBUNTU_RELEASE}/current/unpacked/${UBUNTU_RELEASE}-server-cloudimg-${UBUNTU_ARCH}-${UBUNTU_KERNEL}" -O "${TMPDIR}/vm/kernel"
wget "https://cloud-images.ubuntu.com/${UBUNTU_RELEASE}/current/unpacked/${UBUNTU_RELEASE}-server-cloudimg-${UBUNTU_ARCH}-${UBUNTU_INITRD}" -O "${TMPDIR}/vm/initrd"

msg "Creating cloud-init user data image"
cat << EOF > "${TMPDIR}/vm/user-data"
#!/bin/bash
mkdir /shared
mount -t 9p -o trans=virtio shared /shared -oversion=9p2000.L
echo "--- Machine started up successfully" >> /shared/log
chmod 700 /shared/build_appimage_impl.sh
if /shared/build_appimage_impl.sh "${ARCH}" "${NAME}" &>> /shared/log
then
    touch /shared/success
fi
echo "--- Shutting down the machine" >> /shared/log
poweroff
EOF
cloud-localds "${TMPDIR}/vm/user-data.img" "${TMPDIR}/vm/user-data"

msg "Enlarging VM disk image"
qemu-img resize "${TMPDIR}/vm/disk.img" +20G &> /dev/null

msg "Starting Ubuntu in QEMU"
echo "--- Starting up the machine" >> "${TMPDIR}/shared/log"
"${QEMU}" \
    -m 2048 \
    -smp 2 \
    ${QEMU_ARCH_OPTS} \
    -drive "file=${TMPDIR}/vm/disk.img,format=qcow2,if=none,id=hd0" \
    -drive "file=${TMPDIR}/vm/user-data.img,format=raw,if=none,id=hd1" \
    -device "${QEMU_DISK_DEV},drive=hd0,serial=hd0" \
    -device "${QEMU_DISK_DEV},drive=hd1,serial=hd1" \
    -netdev user,id=net0 \
    -device "${QEMU_NET_DEV},netdev=net0" \
    -kernel "${TMPDIR}/vm/kernel" \
    -initrd "${TMPDIR}/vm/initrd" \
    -append "rw root=/dev/disk/by-id/virtio-hd0-part1" \
    -fsdev "local,path=${TMPDIR}/shared,security_model=mapped-xattr,id=shared,writeout=immediate,fmode=0644,dmode=0755" \
    -device "${QEMU_9P_DEV},fsdev=shared,mount_tag=shared" \
    -display none \
    &> /dev/null &
QEMUPID="${!}"

msg "Building AppImage in QEMU emulated machine"

msg "------------------------------------"
msg "Build log from the QEMU machine:"
msg ""
tail -f "${TMPDIR}/shared/log" -n +1 1>&2 &
TAILPID="${!}"

wait "${QEMUPID}" || true
QEMUPID=""

kill "${TAILPID}" &> /dev/null || true
wait "${TAILPID}" || true
TAILPID=""

msg ""
msg "End of build log from the QEMU machine."
msg "------------------------------------"

msg "QEMU machine shut down successfully, checking build result"
[ -e "${TMPDIR}/shared/success" ]

msg "Writing output ${NAME} file to '${OUTPUT}'"
cp "${TMPDIR}/shared/${NAME}" "${OUTPUT}"

trap - EXIT
rm -rf -- "${TMPDIR}" &> /dev/null 2>&1
msg "Success"
