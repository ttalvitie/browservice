#!/bin/bash

set -e
set -o pipefail
shopt -s expand_aliases

msg() { cat <<< "$@" 1>&2; }

onexit() {
    msg "Building AppImage failed"
}
trap onexit EXIT

msg "Upgrading system"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get upgrade -y

msg "Installing Browservice dependencies"
apt-get install -y wget cmake make g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0 libgtk-3-0

msg "Creating normal user for build"
useradd -m user
alias U="sudo -u user"

msg "Extracting Browservice source"
cd /home/user
U mkdir browservice
cd browservice
U tar xf /shared/src.tar

msg "Downloading CEF"
U bash -c "echo progress=bar:force:noscroll > /home/user/.wgetrc"
U ./download_cef.sh

msg "Setting up CEF"
U ./setup_cef.sh

msg "Compiling Browservice"
U make -j2 release

msg "Creating list of library dependencies"
cd /home/user
U ldd browservice/release/bin/browservice | U grep "=>" | U bash -c "awk '{ print \$3 }' > depstmp"
U ldd browservice/release/bin/retrojsvice.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> depstmp"
U ldd browservice/release/bin/libEGL.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> depstmp"
U ldd browservice/release/bin/libGLESv2.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> depstmp"
U sort depstmp | U grep -v libcef.so | U bash -c "uniq > deps"
U rm depstmp

msg "Result:"
U cat deps

msg "Preparing AppDir"
U mkdir AppDir
U ln -s "usr/bin/browservice" "AppDir/AppRun"
U ln -s "browservice.png" "AppDir/.DirIcon"
U ln -s "usr/share/applications/browservice.desktop" "AppDir/browservice.desktop"
U ln -s "usr/share/icons/hicolor/64x64/apps/browservice.png" "AppDir/browservice.png"

# TODO

trap - EXIT
msg "Success"
