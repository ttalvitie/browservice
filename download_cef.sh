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
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.3%2Bgc53c523%2Bchromium-90.0.4430.93_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="2aeb0cc8b2298d955e6c38649176e10088cf4874"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.3%2Bgc53c523%2Bchromium-90.0.4430.93_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="d8dabbdcb0a9e0d4357fe0c7758f4d0c49ed4857"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.3%2Bgc53c523%2Bchromium-90.0.4430.93_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="d75b28ebf03f9c331636f7f61410b83b080ff858"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.3%2Bgc53c523%2Bchromium-90.0.4430.93_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="9128f6a192003cbbcccd1314488fba50516dbea4"
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
