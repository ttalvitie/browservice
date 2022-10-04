#!/bin/bash

set -e

if [ -z "${1}" ] || ! [ -z "${2}" ]
then
    echo "ERROR: Invalid arguments"
    echo "Usage: setup_cef.sh PATCHED_CEF_TARBALL"
    exit 1
fi

CEFTARBALL="${1}"

if [ -e ./cef ]
then
    echo "./cef already exists!"
    exit 1
fi

if ! [ -f "${CEFTARBALL}" ]
then
    echo "ERROR: Given CEF tarball path '${CEFTARBALL}' is not a file"
    exit 1
fi

ARCH=`uname -m`

if [ "$ARCH" == "x86_64" ] || [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    CMAKEFLAGS=""
elif [ "$ARCH" == "armv7l" ]
then
    CMAKEFLAGS="-DPROJECT_ARCH=arm"
elif [ "$ARCH" == "aarch64" ]
then
    CMAKEFLAGS="-DPROJECT_ARCH=arm64"
else
    echo "ERROR: Unsupported architecture"
    exit 1
fi

echo "Extracting CEF"
mkdir cef
tar xf "${CEFTARBALL}" -C cef --strip-components 1

echo "Building CEF DLL wrapper"

mkdir cef/releasebuild
pushd cef/releasebuild
cmake -DCMAKE_BUILD_TYPE=Release $CMAKEFLAGS ..
make -j2 libcef_dll_wrapper
popd

mkdir cef/debugbuild
pushd cef/debugbuild
cmake -DCMAKE_BUILD_TYPE=Debug $CMAKEFLAGS ..
make -j2 libcef_dll_wrapper
popd

echo "CEF DLL wrapper successfully built"
