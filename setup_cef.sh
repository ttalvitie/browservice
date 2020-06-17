#!/bin/bash

set -e

if [ -e ./cef ]
then
    echo "./cef already exists!"
    exit 1
fi

if ! [ -f ./cef.tar.bz2 ]
then
    echo "./cef.tar.bz2 does not exist -- download it with ./download_cef.sh"
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
    echo "Unsupported architecture"
    exit 1
fi

echo "Extracting CEF"
mkdir cef
tar xf cef.tar.bz2 -C cef --strip-components 1

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
