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
snap install cmake --classic
apt-get install -y wget make g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0 libgtk-3-0 binutils patchelf cabextract libx11-xcb1
apt-get install -y fonts-beng-extra fonts-dejavu-core fonts-deva-extra fonts-droid-fallback fonts-freefont-ttf fonts-gargi fonts-gubbi fonts-gujr-extra fonts-guru-extra fonts-kacst fonts-kacst-one fonts-kalapi fonts-khmeros-core fonts-lao fonts-liberation fonts-liberation2 fonts-lklug-sinhala fonts-lohit-beng-assamese fonts-lohit-beng-bengali fonts-lohit-deva fonts-lohit-gujr fonts-lohit-guru fonts-lohit-knda fonts-lohit-mlym fonts-lohit-orya fonts-lohit-taml fonts-lohit-taml-classical fonts-lohit-telu fonts-nakula fonts-navilu fonts-noto-cjk fonts-noto-color-emoji fonts-noto-mono fonts-opensymbol fonts-orya-extra fonts-pagul fonts-sahadeva fonts-samyak-deva fonts-samyak-gujr fonts-samyak-mlym fonts-samyak-taml fonts-sarai fonts-sil-abyssinica fonts-sil-padauk fonts-smc-anjalioldlipi fonts-smc-chilanka fonts-smc-dyuthi fonts-smc-karumbi fonts-smc-keraleeyam fonts-smc-manjari fonts-smc-meera fonts-smc-rachana fonts-smc-raghumalayalamsans fonts-smc-suruma fonts-smc-uroob fonts-telu-extra fonts-tibetan-machine fonts-tlwg-garuda-ttf fonts-tlwg-kinnari-ttf fonts-tlwg-laksaman-ttf fonts-tlwg-loma-ttf fonts-tlwg-mono-ttf fonts-tlwg-norasi-ttf fonts-tlwg-purisa-ttf fonts-tlwg-sawasdee-ttf fonts-tlwg-typewriter-ttf fonts-tlwg-typist-ttf fonts-tlwg-typo-ttf fonts-tlwg-umpush-ttf fonts-tlwg-waree-ttf fonts-ubuntu gsfonts xfonts-base xfonts-encodings xfonts-scalable xfonts-utils

msg "Setting up CEF"
U PATH="/opt/cmake-3.22.1-linux-x86_64/bin:$PATH" ./setup_cef.sh /shared/cef.tar.bz2

msg "Compiling Browservice"
U make -j2 release

msg "Compiling CEF tests"
U PATH="/opt/cmake-3.22.1-linux-x86_64/bin:$PATH" bash -c "cd cef/releasebuild ; make -j2 ceftests"

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
X11XCBDIR="$(dirname $(whereis libX11-xcb.so.1 | awk '{ print $2 }'))"
for f in \
    browservice/release/bin/browservice \
    browservice/release/bin/retrojsvice.so \
    browservice/release/bin/libcef.so \
    browservice/release/bin/libEGL.so \
    browservice/release/bin/libGLESv2.so \
    browservice/release/bin/chrome-sandbox \
    browservice/release/bin/libvk_swiftshader.so \
    browservice/release/bin/libvulkan.so.1 \
    /usr/bin/Xvfb \
    /usr/bin/xauth \
    /usr/bin/wget \
    /usr/bin/cabextract \
    /usr/bin/sha1sum \
    "${NSSDIR}/libfreebl3.so" \
    "${NSSDIR}/libfreeblpriv3.so" \
    "${NSSDIR}/libnssckbi.so" \
    "${NSSDIR}/libnssdbm3.so" \
    "${NSSDIR}/libsoftokn3.so" \
    "${X11XCBDIR}/libX11-xcb.so"*
do
    U ldd "${f}" | (U grep "=>" || true) | U bash -c "awk '{ print \$3 }' >> deplisttmp"
done

U sort deplisttmp \
    | U grep -v libcef.so \
    | U grep -v libc.so \
    | U grep -v libdl.so \
    | U grep -v libgmp.so \
    | U grep -v libm.so \
    | U grep -v libpthread.so \
    | U grep -v libresolv.so \
    | U grep -v librt.so \
    | U grep -v libstdc++.so \
    | U bash -c "uniq > deplist"
U rm deplisttmp
U mkdir deps
for f in $(cat deplist)
do
    U cp "${f}" deps
done

U bash -c "echo \$(ls deps | sort) > depnamelist"
msg "Library dependencies: $(cat depnamelist)"

EXPECTED_DEPS="libGL.so.1 libGLX.so.0 libGLdispatch.so.0 libPocoFoundation.so.62 libPocoNet.so.62 libX11.so.6 libXau.so.6 libXcomposite.so.1 libXdamage.so.1 libXdmcp.so.6 libXext.so.6 libXfixes.so.3 libXfont2.so.2 libXmuu.so.1 libXrandr.so.2 libXrender.so.1 libasound.so.2 libatk-1.0.so.0 libatk-bridge-2.0.so.0 libatspi.so.0 libaudit.so.1 libavahi-client.so.3 libavahi-common.so.3 libblkid.so.1 libbsd.so.0 libbz2.so.1.0 libcairo.so.2 libcap-ng.so.0 libcom_err.so.2 libcrypto.so.1.1 libcups.so.2 libdatrie.so.1 libdbus-1.so.3 libdrm.so.2 libexpat.so.1 libffi.so.7 libfontconfig.so.1 libfontenc.so.1 libfreetype.so.6 libfribidi.so.0 libgbm.so.1 libgcc_s.so.1 libgcrypt.so.20 libgio-2.0.so.0 libglib-2.0.so.0 libgmodule-2.0.so.0 libgnutls.so.30 libgobject-2.0.so.0 libgpg-error.so.0 libgraphite2.so.3 libgssapi_krb5.so.2 libharfbuzz.so.0 libhogweed.so.5 libidn2.so.0 libjpeg.so.8 libk5crypto.so.3 libkeyutils.so.1 libkrb5.so.3 libkrb5support.so.0 liblz4.so.1 liblzma.so.5 libmount.so.1 libmspack.so.0 libnettle.so.7 libnspr4.so libnss3.so libnssutil3.so libp11-kit.so.0 libpango-1.0.so.0 libpangoft2-1.0.so.0 libpcre.so.3 libpcre2-8.so.0 libpixman-1.so.0 libplc4.so libplds4.so libpng16.so.16 libpsl.so.5 libselinux.so.1 libsmime3.so libsqlite3.so.0 libssl.so.1.1 libsystemd.so.0 libtasn1.so.6 libthai.so.0 libunistring.so.2 libunwind.so.8 libuuid.so.1 libwayland-client.so.0 libwayland-server.so.0 libxcb-render.so.0 libxcb-shm.so.0 libxcb.so.1 libxkbcommon.so.0 libz.so.1"

if [ "$(cat depnamelist)" != "${EXPECTED_DEPS}" ]
then
    msg "Error: Unexpected list of library dependencies; build_appimage_impl.sh must be updated"
    false
fi

msg "Adding run-time library dependencies"
U cp -r "${NSSDIR}" deps/nss
U cp "${X11XCBDIR}/libX11-xcb.so"* deps

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
U cp /usr/bin/Xvfb /usr/bin/xauth /usr/bin/wget /usr/bin/cabextract /usr/bin/sha1sum /shared/run_browservice bin
U chmod 755 bin/run_browservice

msg "Setting up hacks to make Xvfb find the required files inside the AppDir"
U sed -i 's/"%s%sxkbcomp" -w %d %s -xkm "%s" -em1 %s -emp %s -eml %s "%s%s.xkm"/IGNORED="%s%s"%d%s"%s"%s%s%s cp usr\/share\/hack\/xkm "%s%s.xkm"      /g' bin/Xvfb
U sed -i 's/\/usr\/share\/X11\/xkb/usr\/\/share\/X11\/xkb/g' bin/Xvfb
U sed -i 's/\/usr\/share\/fonts/usr\/\/share\/fonts/g' bin/Xvfb
U sed -i 's/\/usr\/lib\/xorg/usr\/\/lib\/xorg/g' bin/Xvfb
U mkdir bin/hack
U mv bin/Xvfb bin/hack/Xvfb
U tee bin/Xvfb << EOF > /dev/null
#!/bin/bash
cd "\$(dirname "\${BASH_SOURCE[0]}")"
cd ../..
exec usr/bin/hack/Xvfb "\$@"
EOF
U chmod +x bin/Xvfb
U mkdir hack
U xkbcomp -xkm - hack/xkm << EOF
xkb_keymap "default" {
    xkb_keycodes             { include "evdev+aliases(qwerty)" };
    xkb_types                { include "complete" };
    xkb_compatibility        { include "complete" };
    xkb_symbols              { include "pc+us+inet(evdev)" };
    xkb_geometry             { include "pc(pc105)" };
};
EOF

msg "Compiling helper executable for fontconfig cache relocation hack"
U gcc /shared/relocate_fontconfig_cache.c -o bin/relocate_fontconfig_cache -O2 -Wall -Wextra -lcrypto

msg "Stripping helper executables"
U strip bin/hack/Xvfb bin/xauth bin/wget bin/cabextract bin/sha1sum bin/relocate_fontconfig_cache

msg "Setting RPATH for helper executables"
U patchelf --set-rpath '$ORIGIN/../../lib' bin/hack/Xvfb
U patchelf --set-rpath '$ORIGIN/../lib' bin/xauth
U patchelf --set-rpath '$ORIGIN/../lib' bin/wget
U patchelf --set-rpath '$ORIGIN/../lib' bin/cabextract
U patchelf --set-rpath '$ORIGIN/../lib' bin/sha1sum
U patchelf --set-rpath '$ORIGIN/../lib' bin/relocate_fontconfig_cache

msg "Recording dependency and font copyright information"
U mkdir doc
for pkg in $((dpkg -S /usr/bin/Xvfb /usr/bin/xauth /usr/bin/wget /usr/bin/cabextract /usr/bin/sha1sum $(cat deplist) $(find /usr/share/fonts/ -type f) $(find "${NSSDIR}" -type f) 2> /dev/null || true) | sed 's/:/ /g' | awk '{ print $1 }' | sort | uniq)
do
    U mkdir "doc/${pkg}"
    U cp "/usr/share/doc/${pkg}/copyright" "doc/${pkg}/copyright"
done

msg "Preparing configuration directory"
U mkdir etc
U cp -Lr /etc/fonts etc/fonts

msg "Creating fontconfig cache for use with the relocation hack"
RANDOM_PATH="/LVT3hhSkcNc5zRqVUNYsPhi04ynbyA6OsunvGAvCq8VDd2RFcLbbHjzU6IXzT47/19GtB0Wo248j5HohiIpAjFJGD6lVfPpPjmUZiyUzY3Xv90dz1n9qjrlXD2rR8EK/dEwsBnoucPaBCN2L3Lrmrf2FcIIeG4puJ28rizYQRX0mofs5CnYiqe8jFGuVJ76/bmC6XM33HnRR9S3QAtMB7iSrQlvT91CMhlTzdmrose41798QltC0TstZpRPIBOL/nhsRVVU6I9VcV0YRg3zz8gqfwe7ZJyaatzrAtjlXK30D07mNnQMD7a9DDcnPp7z/svKScD6FKcpn9fMm0k40BNr6dFqhRyXDU6dkCech3Pp9lqjeuQ4YzbvPqwzowmq/R5X4u2OMXpz6k08a6AcHv6z7TkzfnzsKcq0w303Z6Yz47zOUbZv7TCSuvKvT5LS/IDHXUis1UKrlOqPZkQy2gYVbjdfcfzXtZDm466vseM35dyatcsBcqIqvbpLbz9X/IZznU2HilLp36sEH9jLqdWkScpLOLekIPWSb7gMYP4uwATYhjeM02AXFgH23YkC/p5mAd5HE0Otgsh5gqcdDzzcG7A4umjgX17YqCiFlqTAHUprlCFQePrmE4iqfqmY/lQD5FJflFrWwFIDMRgjhU18yJvrPMvpdpypbt2XPF2sPb18YWUe5wWC6SUAngzO/9wNmfDXrqnlBAmuEuhPvz4d3bvw2BUjhQ56zRn5znvq887C5d8mMm3NrcwX16p9/dY9Kz64wsfnKpaYDN2Y2zvGp7TkHPUYaIsc12FpZc225OaPTyRaZThs9JHJLCln/pqYp5DmZPe68YCf94B8eL73nVXd1KCAgq34qplWtuAHeAQsIrbC7M7ZqGU3OYXH/npKMe4Uj3mORt69rKTOmddFUJBLw6JjYUAFFgnUDbb6OBNvjv2roucHFPACjdPS/PNO2C2YVB8pW5CMP7LgfKUbzXpehDzFjs4q93hS0yiSIzeT6sACnZoEyIGlKwbu"
mkdir -p "$(dirname "${RANDOM_PATH}")"
ln -s / "${RANDOM_PATH}"
U cp -r /etc/fonts fontconfig_config
fontconfreplace() {
    U sed -i "s/${1//\//\\/}/${2//\//\\/}/g" fontconfig_config/fonts.conf
}
fontconfreplace "<dir>/usr/share/fonts</dir>" "<dir>${RANDOM_PATH}/usr/share/fonts</dir>"
fontconfreplace "<dir>/usr/local/share/fonts</dir>" ""
fontconfreplace "<dir prefix=\"xdg\">fonts</dir>" ""
fontconfreplace "<dir>~/.fonts</dir>" ""
fontconfreplace "<cachedir>/var/cache/fontconfig</cachedir>" "<cachedir>/home/user/fontconfig_cache</cachedir>"
fontconfreplace "<cachedir prefix=\"xdg\">fontconfig</cachedir>" ""
fontconfreplace "<cachedir>~/.fontconfig</cachedir>" ""
U mkdir fontconfig_cache
U FONTCONFIG_PATH="/home/user/fontconfig_config" xvfb-run browservice/cef/releasebuild/tests/ceftests/Release/ceftests --gtest_filter=VersionTest.VersionInfo
U FONTCONFIG_PATH="/home/user/fontconfig_config" fc-cache

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
U mkdir AppDir/usr/share/X11
U cp -r /usr/share/X11/xkb AppDir/usr/share/X11/xkb
U mkdir AppDir/usr/lib/xorg
U cp /usr/lib/xorg/protocol.txt AppDir/usr/lib/xorg/protocol.txt
U mkdir -p AppDir/var/cache
U mv fontconfig_cache AppDir/var/cache/fontconfig

U "./${APPIMAGETOOL}" AppDir "${NAME}"
cp "${NAME}" "/shared/${NAME}"

trap - EXIT
msg "Success"
