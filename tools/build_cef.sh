#!/bin/bash
# Based on https://magpcss.org/ceforum/viewtopic.php?f=7&t=17776&p=46448#p47371

set -e
set -o pipefail

msg() { cat <<< "--- $@" 1>&2; }

if [ -z "${1}" ] || [ -z "${2}" ] || [ -z "${3}" ] || ! [ -z "${4}" ]
then
    msg "Invalid arguments"
    msg "Usage: build_cef.sh DOCKER_IMAGE x86_64|armhf|aarch64 BRANCH"
    exit 1
fi

DOCKER_IMAGE="${1}"
ARCH="${2}"
BRANCH="${3}"

if [ "${ARCH}" == "x86_64" ]
then
    AUTOMATE_FLAGS="--x64-build"
    GN_DEFINES="is_official_build=true use_sysroot=true use_allocator=none symbol_level=1 is_cfi=false use_thin_lto=false"
    SYSROOT=""
elif [ "${ARCH}" == "armhf" ]
then
    AUTOMATE_FLAGS="--arm-build"
    GN_DEFINES="is_official_build=true use_sysroot=true use_allocator=none symbol_level=1 is_cfi=false use_thin_lto=false use_vaapi=false"
    SYSROOT="arm"
elif [ "${ARCH}" == "aarch64" ]
then
    AUTOMATE_FLAGS="--arm64-build"
    GN_DEFINES="is_official_build=true use_sysroot=true use_allocator=none symbol_level=1 is_cfi=false use_thin_lto=false"
    SYSROOT="arm64"
else
    msg "ERROR: unsupported architecture '${ARCH}'"
    exit 1
fi

TMPDIR="$(mktemp -d)"
BUILDDIR="${TMPDIR}/build"

msg "Building to '${BUILDDIR}'"
mkdir "${BUILDDIR}"
chmod 777 "${BUILDDIR}"

msg "Fetching automate-git.py"
wget "https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py" -O "${BUILDDIR}/automate-git.py"

msg "Running automate-git.py in Docker"
docker run -e CEF_INSTALL_SYSROOT="${SYSROOT}" -e GN_DEFINES="${GN_DEFINES}" -e CEF_ARCHIVE_FORMAT=tar.bz2 -v "${BUILDDIR}":/build "${DOCKER_IMAGE}" python /build/automate-git.py --download-dir=/build/chromium_git --depot-tools-dir=/build/depot_tools --branch="${BRANCH}" --no-debug-build --minimal-distrib --client-distrib --force-clean --build-target=cefsimple ${AUTOMATE_FLAGS}

msg "CEF successfully built to '${BUILDDIR}'; extract the output tarball and remove the rest of the directory manually (TODO: automatize)"

msg "Success"
