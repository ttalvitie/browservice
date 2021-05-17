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

msg "Installing dependencies"
apt-get install -y wget cmake make g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0 libgtk-3-0 binutils patchelf
apt-get install -y fonts-beng-extra fonts-dejavu-core fonts-deva-extra fonts-droid-fallback fonts-freefont-ttf fonts-gargi fonts-gubbi fonts-gujr-extra fonts-guru-extra fonts-kacst fonts-kacst-one fonts-kalapi fonts-khmeros-core fonts-lao fonts-liberation fonts-liberation2 fonts-lklug-sinhala fonts-lohit-beng-assamese fonts-lohit-beng-bengali fonts-lohit-deva fonts-lohit-gujr fonts-lohit-guru fonts-lohit-knda fonts-lohit-mlym fonts-lohit-orya fonts-lohit-taml fonts-lohit-taml-classical fonts-lohit-telu fonts-nakula fonts-navilu fonts-noto-cjk fonts-noto-color-emoji fonts-noto-mono fonts-opensymbol fonts-orya-extra fonts-pagul fonts-sahadeva fonts-samyak-deva fonts-samyak-gujr fonts-samyak-mlym fonts-samyak-taml fonts-sarai fonts-sil-abyssinica fonts-sil-padauk fonts-smc-anjalioldlipi fonts-smc-chilanka fonts-smc-dyuthi fonts-smc-karumbi fonts-smc-keraleeyam fonts-smc-manjari fonts-smc-meera fonts-smc-rachana fonts-smc-raghumalayalamsans fonts-smc-suruma fonts-smc-uroob fonts-telu-extra fonts-tibetan-machine fonts-tlwg-garuda-ttf fonts-tlwg-kinnari-ttf fonts-tlwg-laksaman-ttf fonts-tlwg-loma-ttf fonts-tlwg-mono-ttf fonts-tlwg-norasi-ttf fonts-tlwg-purisa-ttf fonts-tlwg-sawasdee-ttf fonts-tlwg-typewriter-ttf fonts-tlwg-typist-ttf fonts-tlwg-typo-ttf fonts-tlwg-umpush-ttf fonts-tlwg-waree-ttf fonts-ubuntu gsfonts xfonts-base xfonts-encodings xfonts-scalable xfonts-utils

msg "Downloading CEF"
U bash -c "echo progress=bar:force:noscroll > /home/user/.wgetrc"
U ./download_cef.sh

msg "Setting up CEF"
U ./setup_cef.sh

msg "Compiling Browservice"
U make -j2 release

msg "Stripping Browservice binaries"
U strip release/bin/browservice release/bin/retrojsvice.so release/bin/libcef.so release/bin/libEGL.so release/bin/libGLESv2.so release/bin/chrome-sandbox

msg "Setting RPATH for Browservice binaries"
U patchelf --set-rpath '.:$ORIGIN:$ORIGIN/../../usr/lib' release/bin/browservice
U patchelf --set-rpath '$ORIGIN/../../usr/lib' release/bin/retrojsvice.so
U patchelf --set-rpath '$ORIGIN/../../usr/lib' release/bin/libcef.so
U patchelf --set-rpath '$ORIGIN/../../usr/lib' release/bin/libEGL.so
U patchelf --set-rpath '$ORIGIN/../../usr/lib' release/bin/libGLESv2.so
U patchelf --set-rpath '$ORIGIN/../../usr/lib' release/bin/chrome-sandbox

msg "Collecting library dependencies"
cd /home/user
NSSDIR="$(dirname $(whereis libnss3.so | awk '{ print $2 }'))/nss"
for f in \
    browservice/release/bin/browservice \
    browservice/release/bin/retrojsvice.so \
    browservice/release/bin/libcef.so \
    browservice/release/bin/libEGL.so \
    browservice/release/bin/libGLESv2.so \
    browservice/release/bin/chrome-sandbox \
    /usr/bin/Xvfb \
    /usr/bin/xauth \
    "${NSSDIR}/libfreebl3.so" \
    "${NSSDIR}/libfreeblpriv3.so" \
    "${NSSDIR}/libnssckbi.so" \
    "${NSSDIR}/libnssdbm3.so" \
    "${NSSDIR}/libsoftokn3.so"
do
    U ldd "${f}" | U grep "=>" | U bash -c "awk '{ print \$3 }' >> deplisttmp"
done

U sort deplisttmp | U grep -v libcef.so | U bash -c "uniq > deplist"
U rm deplisttmp
U mkdir deps
for f in $(cat deplist)
do
    U cp "${f}" deps
done

U bash -c "echo \$(ls deps | sort) > deplist"
msg "All library dependencies: $(cat deplist)"

if [ "$(cat deplist)" != "libGL.so.1 libGLX.so.0 libGLdispatch.so.0 libPocoCrypto.so.50 libPocoFoundation.so.50 libPocoNet.so.50 libX11.so.6 libXau.so.6 libXcomposite.so.1 libXcursor.so.1 libXdamage.so.1 libXdmcp.so.6 libXext.so.6 libXfixes.so.3 libXfont2.so.2 libXi.so.6 libXinerama.so.1 libXmuu.so.1 libXrandr.so.2 libXrender.so.1 libasound.so.2 libatk-1.0.so.0 libatk-bridge-2.0.so.0 libatspi.so.0 libaudit.so.1 libavahi-client.so.3 libavahi-common.so.3 libblkid.so.1 libbsd.so.0 libbz2.so.1.0 libc.so.6 libcairo-gobject.so.2 libcairo.so.2 libcap-ng.so.0 libcom_err.so.2 libcrypto.so.1.1 libcups.so.2 libdatrie.so.1 libdbus-1.so.3 libdl.so.2 libdrm.so.2 libepoxy.so.0 libexpat.so.1 libffi.so.6 libfontconfig.so.1 libfontenc.so.1 libfreetype.so.6 libgbm.so.1 libgcc_s.so.1 libgcrypt.so.20 libgdk-3.so.0 libgdk_pixbuf-2.0.so.0 libgio-2.0.so.0 libglib-2.0.so.0 libgmodule-2.0.so.0 libgmp.so.10 libgnutls.so.30 libgobject-2.0.so.0 libgpg-error.so.0 libgraphite2.so.3 libgssapi_krb5.so.2 libgtk-3.so.0 libharfbuzz.so.0 libhogweed.so.4 libidn2.so.0 libjpeg.so.8 libk5crypto.so.3 libkeyutils.so.1 libkrb5.so.3 libkrb5support.so.0 liblz4.so.1 liblzma.so.5 libm.so.6 libmount.so.1 libnettle.so.6 libnspr4.so libnss3.so libnssutil3.so libp11-kit.so.0 libpango-1.0.so.0 libpangocairo-1.0.so.0 libpangoft2-1.0.so.0 libpcre.so.3 libpixman-1.so.0 libplc4.so libplds4.so libpng16.so.16 libpthread.so.0 libresolv.so.2 librt.so.1 libselinux.so.1 libsmime3.so libsqlite3.so.0 libssl.so.1.1 libstdc++.so.6 libsystemd.so.0 libtasn1.so.6 libthai.so.0 libunistring.so.2 libuuid.so.1 libwayland-client.so.0 libwayland-cursor.so.0 libwayland-egl.so.1 libwayland-server.so.0 libxcb-render.so.0 libxcb-shm.so.0 libxcb.so.1 libxkbcommon.so.0 libxshmfence.so.1 libz.so.1" ]
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
U rm deps/libstdc++.so.*

U bash -c "echo \$(ls deps | sort) > deplist"
msg "Filtered library dependencies: $(cat deplist)"

msg "Adding run-time library dependencies"
U cp -r "${NSSDIR}" deps/nss

msg "Stripping library dependencies"
U strip deps/*.so*
U strip deps/nss/*.so*

msg "Setting RPATH for library dependencies"
for f in deps/*.so*
do
    U patchelf --set-rpath '$ORIGIN' "${f}"
done
for f in deps/nss/*.so*
do
    U patchelf --set-rpath '../$ORIGIN' "${f}"
done

msg "Collecting helper executables"
U mkdir bin
U cp /usr/bin/Xvfb /usr/bin/xauth /shared/run_browservice bin
U chmod 755 bin/run_browservice

msg "Setting up hack where Xvfb is binary patched to use fixed xkm file to manage without xkbcomp"
POS="$(grep -FobUa '"%s%sxkbcomp" -w %d %s -xkm "%s" -em1 %s -emp %s -eml %s "%s%s.xkm"' bin/Xvfb)"
POS="${POS%:\"%s%sxkbcomp\" -w %d %s -xkm \"%s\" -em1 %s -emp %s -eml %s \"%s%s.xkm\"}"
echo -n 'IGNORED="%s%s"%d%s"%s"%s%s%s write_default_xkm.sh "%s%s.xkm"       ' | dd bs=1 of=bin/Xvfb seek="${POS}" conv=notrunc
U tee bin/write_default_xkm.sh << EOF > /dev/null
#!/bin/sh
SCRIPTPATH="\$(readlink -f "\$0")"
SCRIPTDIR="\$(dirname "\${SCRIPTPATH}")"
cat "\${SCRIPTDIR}/../share/hack/default.xkm" > "\$1"
EOF
U chmod +x bin/write_default_xkm.sh
U mkdir hack
U xkbcomp -xkm - hack/default.xkm << EOF
xkb_keymap "default" {
    xkb_keycodes             { include "evdev+aliases(qwerty)" };
    xkb_types                { include "complete" };
    xkb_compatibility        { include "complete" };
    xkb_symbols              { include "pc+us+inet(evdev)" };
    xkb_geometry             { include "pc(pc105)" };
};
EOF

msg "Stripping helper executables"
U strip bin/Xvfb bin/xauth

msg "Setting RPATH for helper executables"
U patchelf --set-rpath '$ORIGIN/../lib' bin/Xvfb
U patchelf --set-rpath '$ORIGIN/../lib' bin/xauth

msg "Recording font copyright information"
U mkdir doc
for pkg in $((dpkg -S $(find /usr/share/fonts/ -type f) 2> /dev/null || true) | awk '{ print $1 }' | sort | uniq)
do
    pkg="${pkg%:}"
    U mkdir "doc/${pkg}"
    U cp "/usr/share/doc/${pkg}/copyright" "doc/${pkg}/copyright"
done

msg "Preparing configuration directory"
U mkdir etc
U cp -Lr /etc/fonts etc/fonts

msg "Preparing AppDir"
U mkdir AppDir
U ln -s usr/bin/run_browservice AppDir/AppRun
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
U mkdir AppDir/opt
U mv browservice/release/bin AppDir/opt/browservice
U mv deps AppDir/usr/lib
U mv bin AppDir/usr/bin
U mv hack AppDir/usr/share/hack
U mv doc AppDir/usr/share/doc
U cp -r /usr/share/fonts AppDir/usr/share/fonts
U mv etc AppDir/etc

U "./${APPIMAGETOOL}" AppDir "${NAME}"
cp "${NAME}" "/shared/${NAME}"

trap - EXIT
msg "Success"
