#!/bin/bash

set -e

if [ -e ./cef.tar.bz2 ]
then
    echo "./cef.tar.bz2 already exists!"
    exit 1
fi

ARCH=`uname -m`

if [ "$ARCH" == "x86_64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_103.0.12%2Bg8eb56c7%2Bchromium-103.0.5060.134_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="807be2589b43a5d52a15e62639ed345200d14fff"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_103.0.12%2Bg8eb56c7%2Bchromium-103.0.5060.134_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="45676a1d4b455a7446d2be5b8dc474423e61cf14"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_103.0.12%2Bg8eb56c7%2Bchromium-103.0.5060.134_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="bd53efc658322b5eb2b4466784e6e9411a08015b"
else
    echo "Unsupported architecture"
    exit 1
fi

if echo "${SHA1}  cef_unverified.tar.bz2" | sha1sum -c
then
    mv cef_unverified.tar.bz2 cef.tar.bz2
    echo "Successfully downloaded and verified cef.tar.bz2"
else
    echo "The downloaded file cef_unverified.tar.bz2 did not pass the integrity test"
    exit 1
fi
