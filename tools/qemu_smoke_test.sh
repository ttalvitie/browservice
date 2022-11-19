#!/bin/bash

set -e
set -o pipefail

msg() { cat <<< "--- $@" 1>&2; }

if [ -z "${1}" ] || [ -z "${2}" ] || ! [ -z "${3}" ]
then
    msg "Invalid arguments"
    msg "Usage: qemu_smoke_test.sh <config> <appimage>"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

if ! [ -f "${SCRIPT_DIR}/smoke_test.py" ] || ! [ -d "${SCRIPT_DIR}/smoke_test_data" ]
then
    msg "Smoke test script '${SCRIPT_DIR}/smoke_test.py' or data '${SCRIPT_DIR}/smoke_test_data' not found"
    exit 1
fi

CONFIG="${1}"
APPIMAGE="${2}"

CONFIGFILE="${SCRIPT_DIR}/qemu_smoke_test_data/config_${CONFIG}.sh"
if ! [ -f "${CONFIGFILE}" ]
then
    msg "Config file '${CONFIGFILE}' does not exist"
    exit 1
fi

if ! [ -f "${APPIMAGE}" ]
then
    msg "Input AppImage '${APPIMAGE}' not found"
    exit 1
fi

for QEMU in qemu-system-x86_64 qemu-system-arm qemu-system-aarch64
do
    if ! "${QEMU}" --version &> /dev/null
    then
        msg "ERROR: running '${QEMU} --version' resulted in nonzero exit status"
        exit 1
    fi
done

for COMMAND in cloud-localds unxz 7z openssl base64
do
    if ! command -v "${COMMAND}" &> /dev/null
    then
        msg "ERROR: command '${COMMAND}' does not exist"
        exit 1
    fi
done

AARCH64_EFI_PATH="/usr/share/qemu-efi-aarch64/QEMU_EFI.fd"
if ! [ -e "${AARCH64_EFI_PATH}" ]
then
    msg "ERROR: aarch64 EFI image '${AARCH64_EFI_PATH}' does not exist"
    exit 1
fi

TMPDIR="$(mktemp -d)"
QEMUPID=""
TAILPID=""

onexit() {
    msg "Running smoke test in QEMU failed"
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

msg "Reading config '${CONFIG}'"

ARCH=""
RUNNER=""
IMAGE_URL=""
KERNEL_URL=""
INITRD_URL=""
USE_AARCH64_EFI=""
RASPI=""

. "${CONFIGFILE}"

for VAR in ARCH RUNNER IMAGE_URL
do
    if [ -z "${!VAR}" ]
    then
        msg "Config '${CONFIG}' did not set required variable '${VAR}'"
        exit 1
    fi
done

RUNNERFILE="${SCRIPT_DIR}/qemu_smoke_test_data/runner_${RUNNER}.sh"
if ! [ -f "${RUNNERFILE}" ]
then
    msg "ERROR: Runner '${RUNNERFILE}' does not exist"
    exit 1
fi

if [ "${ARCH}" == "x86_64" ]
then
    QEMU="qemu-system-x86_64"
    QEMU_ARCH_OPTS=()
    QEMU_DISK_DEV="virtio-blk-pci"
    QEMU_9P_DEV="virtio-9p-pci"
    QEMU_NET_DEV="virtio-net-pci"
elif [ "${ARCH}" == "armhf" ]
then
    QEMU="qemu-system-arm"
    QEMU_ARCH_OPTS=(-machine virt)
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
    QEMU_NET_DEV="virtio-net-device"
elif [ "${ARCH}" == "aarch64" ]
then
    QEMU="qemu-system-aarch64"
    QEMU_ARCH_OPTS=(-machine virt -cpu cortex-a57)
    QEMU_DISK_DEV="virtio-blk-device"
    QEMU_9P_DEV="virtio-9p-device"
    QEMU_NET_DEV="virtio-net-device"
else
    msg "ERROR: unsupported architecture '${ARCH}' in config"
    exit 1
fi

msg "Populating shared directory for QEMU machine"
mkdir "${TMPDIR}/shared"
touch "${TMPDIR}/shared/log"
cp "${APPIMAGE}" "${TMPDIR}/shared/browservice.AppImage"
cp "${SCRIPT_DIR}/smoke_test.py" "${TMPDIR}/shared/smoke_test.py"
cp -r "${SCRIPT_DIR}/smoke_test_data" "${TMPDIR}/shared/smoke_test_data"
cp "${RUNNERFILE}" "${TMPDIR}/shared/runner.sh"
pushd "${TMPDIR}" &> /dev/null
sha1sum shared/runner.sh > shared/runner.sh.sha1
popd &> /dev/null

msg "Downloading disk, BIOS, kernel and initrd images"
mkdir "${TMPDIR}/vm"
wget "${IMAGE_URL}" -O "${TMPDIR}/vm/disk.img"

QEMU_BIOS_OPTS=()
if ! [ -z "${USE_AARCH64_EFI}" ]
then
    QEMU_BIOS_OPTS+=(-bios "${AARCH64_EFI_PATH}")
fi

QEMU_KERNEL_OPTS=()
if ! [ -z "${KERNEL_URL}" ]
then
    wget "${KERNEL_URL}" -O "${TMPDIR}/vm/kernel"
    QEMU_KERNEL_OPTS+=(-kernel "${TMPDIR}/vm/kernel")
    QEMU_KERNEL_OPTS+=(-append "rw root=/dev/disk/by-id/virtio-hd0-part1")
fi
if ! [ -z "${INITRD_URL}" ]
then
    wget "${INITRD_URL}" -O "${TMPDIR}/vm/initrd"
    QEMU_KERNEL_OPTS+=(-initrd "${TMPDIR}/vm/initrd")
fi

if [ -z "${RASPI}" ]
then
    msg "Creating cloud-init user data image"
    cat << EOF > "${TMPDIR}/vm/user-data"
#!/bin/bash
mkdir /shared
mount -t 9p -o trans=virtio,version=9p2000.L,msize=10485760 shared /shared
cd /
if sha1sum -c shared/runner.sh.sha1
then
    echo "--- Machine started up successfully" >> /shared/log
    chmod 700 /shared/runner.sh
    if /shared/runner.sh &>> /shared/log
    then
        touch /shared/success
    fi
fi
echo "--- Shutting down the machine" >> /shared/log
poweroff
EOF
    cloud-localds "${TMPDIR}/vm/user-data.img" "${TMPDIR}/vm/user-data"

    msg "Enlarging VM disk image"
    qemu-img resize "${TMPDIR}/vm/disk.img" +2G &> /dev/null

    msg "Starting image in QEMU"
    echo "--- Starting up the machine" >> "${TMPDIR}/shared/log"
    "${QEMU}" \
        -m 2048 \
        -smp 2 \
        "${QEMU_ARCH_OPTS[@]}" \
        "${QEMU_BIOS_OPTS[@]}" \
        -drive "file=${TMPDIR}/vm/disk.img,format=qcow2,if=none,id=hd0" \
        -drive "file=${TMPDIR}/vm/user-data.img,format=raw,if=none,id=hd1" \
        -device "${QEMU_DISK_DEV},drive=hd0,serial=hd0" \
        -device "${QEMU_DISK_DEV},drive=hd1,serial=hd1" \
        -netdev user,id=net0 \
        -device "${QEMU_NET_DEV},netdev=net0" \
        "${QEMU_KERNEL_OPTS[@]}" \
        -fsdev "local,path=${TMPDIR}/shared,security_model=mapped-xattr,id=shared,writeout=immediate,fmode=0644,dmode=0755" \
        -device "${QEMU_9P_DEV},fsdev=shared,mount_tag=shared" \
        -display none \
        &> /dev/null &
    QEMUPID="${!}"
else
    msg "Decompressing disk image"
    mv "${TMPDIR}/vm/disk.img" "${TMPDIR}/vm/disk.img.xz"
    unxz "${TMPDIR}/vm/disk.img.xz"

    msg "Extracting kernel and device tree from disk image"
    pushd "${TMPDIR}/vm" &> /dev/null
    7z x disk.img 0.fat &> /dev/null
    7z x 0.fat kernel8.img &> /dev/null
    7z x 0.fat bcm2710-rpi-3-b.dtb &> /dev/null
    popd &> /dev/null

    msg "Creating tarball for shared data"
    pushd "${TMPDIR}" &> /dev/null
    tar cf vm/shared.tar shared
    SHARED_TARBALL_SIZE="$(stat -c%s vm/shared.tar)"
    popd &> /dev/null

    msg "Enlarging disk image and injecting shared data tarball there"
    truncate -s 16G "${TMPDIR}/vm/disk.img"
    cat "${TMPDIR}/vm/shared.tar" >> "${TMPDIR}/vm/disk.img"
    truncate -s 32G "${TMPDIR}/vm/disk.img"

    msg "Creating kernel command line with script injections"
    cat << EOF > "${TMPDIR}/vm/startup.sh"
#!/bin/bash
rm /etc/systemd/system/multi-user.target.wants/{userconfig.service,regenerate_ssh_host_keys.service,rpi-eeprom-update.service}
rm /etc/systemd/system/timers.target.wants/apt-*
rm /etc/rc3.d/S01resize2fs_once
exit 0
EOF

    SUCCESS_TOKEN="SUCCESS_TOKEN_$(openssl rand -hex 32)"

    cat << EOF > "${TMPDIR}/vm/rc.local"
#!/bin/bash
for i in {1..30}; do ping -q -c 1 -W 1 10.0.2.2 && break ; sleep 1 ; done
parted -s -a opt /dev/mmcblk0 "resizepart 2 8G"
resize2fs /dev/mmcblk0p2
echo "CONF_SWAPSIZE=1024" >> /etc/dphys-swapfile
systemctl restart dphys-swapfile.service
cd /
dd if=/dev/mmcblk0 iflag=skip_bytes,count_bytes,fullblock bs=1M skip=17179869184 count=${SHARED_TARBALL_SIZE} | tar x
if sha1sum -c shared/runner.sh.sha1
then
    echo "--- Machine started up successfully" > /dev/ttyAMA0
    chmod 700 /shared/runner.sh
    if /shared/runner.sh &> /dev/ttyAMA0
    then
        echo "${SUCCESS_TOKEN}" > /dev/ttyAMA0
    fi
fi
echo "--- Shutting down the machine" > /dev/ttyAMA0
poweroff
EOF

    KERNELCMDLINE="rw root=/dev/mmcblk0p2 rootfstype=ext4 rootwait init=/bin/bash -- -c \"echo $(cat ${TMPDIR}/vm/startup.sh | base64 -w0) | base64 -d | bash ; echo $(cat ${TMPDIR}/vm/rc.local | base64 -w0) | base64 -d > /etc/rc.local ; exec /sbin/init\""

    msg "Starting image in QEMU"
    qemu-system-aarch64 \
        -machine raspi3b \
        -m 1024 \
        -kernel "${TMPDIR}/vm/kernel8.img" \
        -append "${KERNELCMDLINE}" \
        -dtb "${TMPDIR}/vm/bcm2710-rpi-3-b.dtb" \
        -drive "id=hd-root,file=${TMPDIR}/vm/disk.img,format=raw" \
        -serial "file:${TMPDIR}/shared/log" \
        -netdev user,id=net0 \
        -device usb-net,netdev=net0 \
        -display none \
        &> /dev/null &
    QEMUPID="${!}"
fi

msg "Running smoke test in QEMU emulated machine"

msg "------------------------------------"
msg "Test log from the QEMU machine:"
msg ""
tail -f "${TMPDIR}/shared/log" -n +1 1>&2 &
TAILPID="${!}"

wait "${QEMUPID}" || true
QEMUPID=""

kill "${TAILPID}" &> /dev/null || true
wait "${TAILPID}" || true
TAILPID=""

msg ""
msg "End of test log from the QEMU machine."
msg "------------------------------------"

msg "QEMU machine shut down successfully, checking test result"
STATUS=1
if [ -z "${RASPI}" ]
then
    if [ -e "${TMPDIR}/shared/success" ]
    then
        STATUS=0
    fi
else
    if grep -q "${SUCCESS_TOKEN}" "${TMPDIR}/shared/log"
    then
        STATUS=0
    fi
fi

trap - EXIT
rm -rf -- "${TMPDIR}" &> /dev/null 2>&1

if [ "${STATUS}" == "0" ]
then
    msg "Smoke test succeeded"
else
    msg "Smoke test failed"
fi

exit ${STATUS}
