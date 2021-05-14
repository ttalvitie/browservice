#!/bin/bash

set -e
set -o pipefail
shopt -s expand_aliases

msg() { cat <<< "--- $@" 1>&2; }

if [ -z "${1}" ] || [ -z "${2}" ] || ! [ -z "${3}" ]
then
    msg "Invalid arguments"
    msg "Usage: build_appimage.sh ARCH NAME"
    exit 1
fi

ARCH="${1}"
NAME="${2}"

if [ "${ARCH}" == "x86_64" ]
then
    APPIMAGETOOL="appimagetool-x86_64.AppImage"
elif [ "${ARCH}" == "i386" ]
then
    APPIMAGETOOL="appimagetool-i686.AppImage"
elif [ "${ARCH}" == "armhf" ]
then
    APPIMAGETOOL="appimagetool-armhf.AppImage"
elif [ "${ARCH}" == "aarch64" ]
then
    APPIMAGETOOL="appimagetool-aarch64.AppImage"
else
    msg "ERROR: unsupported architecture '${ARCH}'"
    exit 1
fi

onexit() {
    msg "Building AppImage failed"
}
trap onexit EXIT

msg "Creating normal user for build"
useradd -m user
alias U="sudo -u user"

msg "Downloading appimagetool"
cd /home/user
U wget "https://github.com/AppImage/AppImageKit/releases/download/continuous/${APPIMAGETOOL}"
U chmod +x "${APPIMAGETOOL}"

msg "Extracting Browservice source"
U mkdir browservice
cd browservice
U tar xf /shared/src.tar

msg "Upgrading system"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get upgrade -y

msg "Installing Browservice dependencies"
apt-get install -y wget cmake make g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0 libgtk-3-0 binutils

msg "Downloading CEF"
U bash -c "echo progress=bar:force:noscroll > /home/user/.wgetrc"
U ./download_cef.sh

msg "Setting up CEF"
U ./setup_cef.sh

msg "Compiling Browservice"
U make -j2 release

msg "Stripping Browservice binaries"
U strip release/bin/browservice release/bin/retrojsvice.so release/bin/libEGL.so release/bin/libGLESv2.so

msg "Collecting binary dependencies"
cd /home/user
U ldd browservice/release/bin/browservice | U grep "=>" | U bash -c "awk '{ print \$3 }' > deplisttmp"
U ldd browservice/release/bin/retrojsvice.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd browservice/release/bin/libEGL.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd browservice/release/bin/libGLESv2.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U sort deplisttmp | U grep -v libcef.so | U bash -c "uniq > deplist"
U rm deplisttmp
U mkdir deps
for f in $(cat deplist)
do
    U cp "${f}" deps
done

U bash -c "echo \$(ls deps | sort) > deplist"
msg "All binary dependencies: $(cat deplist)"

msg "Filtering binary dependencies"
U rm deps/libc.so.*

U bash -c "echo \$(ls deps | sort) > deplist"
msg "Filtered binary dependencies: $(cat deplist)"

msg "Stripping binary dependencies"
U strip deps/*

msg "Preparing AppDir"
U mkdir AppDir
U ln -s usr/bin/browservice AppDir/AppRun
U ln -s browservice.png AppDir/.DirIcon
U ln -s usr/share/applications/browservice.desktop AppDir/browservice.desktop
U ln -s usr/share/icons/hicolor/32x32/apps/browservice.png AppDir/browservice.png
U mkdir -p AppDir/usr/share/icons/hicolor/16x16/apps
U mkdir -p AppDir/usr/share/icons/hicolor/32x32/apps
U mkdir -p AppDir/usr/share/icons/hicolor/64x64/apps
U mkdir -p AppDir/usr/share/icons/hicolor/128x128/apps
U mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps
U mkdir -p AppDir/usr/share/icons/hicolor/scalable/apps
U cp /shared/browservice.png AppDir/usr/share/icons/hicolor/32x32/apps
U mkdir -p AppDir/usr/share/applications
U cp /shared/browservice.desktop AppDir/usr/share/applications/browservice.desktop
U mv browservice/release/bin AppDir/usr/bin
U mv deps AppDir/usr/lib

U "./${APPIMAGETOOL}" AppDir "${NAME}"
cp "${NAME}" "/shared/${NAME}"

trap - EXIT
msg "Success"
