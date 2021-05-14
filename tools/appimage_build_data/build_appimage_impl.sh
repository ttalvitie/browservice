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
U strip release/bin/browservice release/bin/retrojsvice.so release/bin/libcef.so release/bin/libEGL.so release/bin/libGLESv2.so

msg "Collecting library dependencies"
cd /home/user
U ldd browservice/release/bin/browservice | U grep "=>" | U bash -c "awk '{ print \$3 }' > deplisttmp"
U ldd browservice/release/bin/retrojsvice.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd browservice/release/bin/libcef.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd browservice/release/bin/libEGL.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd browservice/release/bin/libGLESv2.so | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U ldd /usr/bin/Xvfb | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
U sort deplisttmp | U grep -v libcef.so | U bash -c "uniq > deplist"
U rm deplisttmp
U mkdir deps
for f in $(cat deplist)
do
    U cp "${f}" deps
done

U bash -c "echo \$(ls deps | sort) > deplist"
msg "All library dependencies: $(cat deplist)"

if [ "$(cat deplist)" != "libGL.so.1 libGLX.so.0 libGLdispatch.so.0 libPocoCrypto.so.50 libPocoFoundation.so.50 libPocoNet.so.50 libX11.so.6 libXau.so.6 libXcomposite.so.1 libXcursor.so.1 libXdamage.so.1 libXdmcp.so.6 libXext.so.6 libXfixes.so.3 libXfont2.so.2 libXi.so.6 libXinerama.so.1 libXrandr.so.2 libXrender.so.1 libasound.so.2 libatk-1.0.so.0 libatk-bridge-2.0.so.0 libatspi.so.0 libaudit.so.1 libavahi-client.so.3 libavahi-common.so.3 libblkid.so.1 libbsd.so.0 libbz2.so.1.0 libc.so.6 libcairo-gobject.so.2 libcairo.so.2 libcap-ng.so.0 libcom_err.so.2 libcrypto.so.1.1 libcups.so.2 libdatrie.so.1 libdbus-1.so.3 libdl.so.2 libdrm.so.2 libepoxy.so.0 libexpat.so.1 libffi.so.6 libfontconfig.so.1 libfontenc.so.1 libfreetype.so.6 libgbm.so.1 libgcc_s.so.1 libgcrypt.so.20 libgdk-3.so.0 libgdk_pixbuf-2.0.so.0 libgio-2.0.so.0 libglib-2.0.so.0 libgmodule-2.0.so.0 libgmp.so.10 libgnutls.so.30 libgobject-2.0.so.0 libgpg-error.so.0 libgraphite2.so.3 libgssapi_krb5.so.2 libgtk-3.so.0 libharfbuzz.so.0 libhogweed.so.4 libidn2.so.0 libjpeg.so.8 libk5crypto.so.3 libkeyutils.so.1 libkrb5.so.3 libkrb5support.so.0 liblz4.so.1 liblzma.so.5 libm.so.6 libmount.so.1 libnettle.so.6 libnspr4.so libnss3.so libnssutil3.so libp11-kit.so.0 libpango-1.0.so.0 libpangocairo-1.0.so.0 libpangoft2-1.0.so.0 libpcre.so.3 libpixman-1.so.0 libplc4.so libplds4.so libpng16.so.16 libpthread.so.0 libresolv.so.2 librt.so.1 libselinux.so.1 libsmime3.so libssl.so.1.1 libstdc++.so.6 libsystemd.so.0 libtasn1.so.6 libthai.so.0 libunistring.so.2 libuuid.so.1 libwayland-client.so.0 libwayland-cursor.so.0 libwayland-egl.so.1 libwayland-server.so.0 libxcb-render.so.0 libxcb-shm.so.0 libxcb.so.1 libxkbcommon.so.0 libxshmfence.so.1 libz.so.1" ]
then
    msg "Error: Unexpected list of library dependencies; build_appimage_impl.sh must be updated"
    false
fi

msg "Filtering library dependencies"
U rm deps/libc.so.*
U rm deps/libdl.so.*
U rm deps/libgmp.so.1*
U rm deps/libidn2.so.*
U rm deps/libm.so.*
U rm deps/libpthread.so.*
U rm deps/libresolv.so.*
U rm deps/librt.so.*
U rm deps/libselinux.so.*

U bash -c "echo \$(ls deps | sort) > deplist"
msg "Filtered library dependencies: $(cat deplist)"

msg "Stripping library dependencies"
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
U cp /usr/bin/Xvfb AppDir/usr/bin/Xvfb
U strip AppDir/usr/bin/Xvfb

U "./${APPIMAGETOOL}" AppDir "${NAME}"
cp "${NAME}" "/shared/${NAME}"

trap - EXIT
msg "Success"
