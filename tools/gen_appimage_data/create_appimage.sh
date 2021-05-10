#!/bin/bash

set -e
set -o pipefail

msg() { cat <<< "$@" 1>&2; }

if [ -z "$1" ] || ! [ -z "$2" ]
then
        msg "Invalid arguments"
        msg "Usage: <source bin directory>"
        exit 1
fi

SRCDIR="${1}"

if [ "$UID" -eq "0" ]
then
    msg "ERROR: running as root"
    exit 1
fi

if ! [ -d "${SRCDIR}" ]
then
    msg "ERROR: ${SRCDIR} is not a directory"
    exit 1
fi

if ! [ -x "${SRCDIR}/browservice" ] || [ "$(stat -c "%u" -- "${SRCDIR}/browservice")" != "${UID}" ]
then
    msg "ERROR: ${SRCDIR}/browservice is not an executable file owned by the current user"
    exit 1
fi

if ! [ -x "${SRCDIR}/retrojsvice.so" ] || [ "$(stat -c "%u" -- "${SRCDIR}/retrojsvice.so")" != "${UID}" ]
then
    msg "ERROR: ${SRCDIR}/retrojsvice.so is not an executable file owned by the current user"
    exit 1
fi

if ! [ -x "${SRCDIR}/libcef.so" ] || [ "$(stat -c "%u" -- "${SRCDIR}/libcef.so")" != "${UID}" ]
then
    msg "ERROR: ${SRCDIR}/libcef.so is not an executable file owned by the current user"
    exit 1
fi

TMPDIR="$(mktemp -d)"

onexit() {
    msg "Creating AppImage failed"
    rm -rf -- "${TMPDIR}" > /dev/null 2>&1 || true
}
trap onexit EXIT

APPDIR="${TMPDIR}/AppDir"
mkdir "${APPDIR}"
ln -s "usr/bin/browservice" "${APPDIR}/AppRun"
ln -s "browservice.png" "${APPDIR}/.DirIcon"
ln -s "usr/share/applications/browservice.desktop" "${APPDIR}/browservice.desktop"
ln -s "usr/share/icons/hicolor/64x64/apps/browservice.png" "${APPDIR}/browservice.png"

ldd "${SRCDIR}/browservice" | grep "=>" | awk '{ print $3 }' > "${TMPDIR}/depstmp"
ldd "${SRCDIR}/retrojsvice.so" | grep "=>" | awk '{ print $3 }' >> "${TMPDIR}/depstmp"
ldd "${SRCDIR}/libEGL.so" | grep "=>" | awk '{ print $3 }' >> "${TMPDIR}/depstmp"
ldd "${SRCDIR}/libGLESv2.so" | grep "=>" | awk '{ print $3 }' >> "${TMPDIR}/depstmp"
sort "${TMPDIR}/depstmp" | grep -v libcef.so | uniq > "${TMPDIR}/deps"
rm "${TMPDIR}/depstmp"

# TODO

cat "${TMPDIR}/deps"

trap - EXIT
rm -rf -- "${TMPDIR}" > /dev/null 2>&1
msg "Success"
