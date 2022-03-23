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
    wget "https://cef-builds.spotifycdn.com/cef_binary_99.2.13%2Bgd9af866%2Bchromium-99.0.4844.83_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="d9581978a60f78b64db596915beedc60e5732bf6"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_99.2.13%2Bgd9af866%2Bchromium-99.0.4844.83_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="94cfda8760f042d53662d23e5c6ed20f55cd6d88"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_99.2.13%2Bgd9af866%2Bchromium-99.0.4844.83_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="928f4423a19d8f4a2cd2c0550d05a8e077ba95f2"
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
