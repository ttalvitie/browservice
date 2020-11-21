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
    wget "https://cef-builds.spotifycdn.com/cef_binary_87.1.6%2Bg315d248%2Bchromium-87.0.4280.66_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="d683d27b42237b43004578db84d48d37c6ef4511"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_87.1.6%2Bg315d248%2Bchromium-87.0.4280.66_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="1502c2cc41c144a70a0bd7c9b1a4347bfc48fd3a"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_87.1.6%2Bg315d248%2Bchromium-87.0.4280.66_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="c5b5e1883cf9e56c7c391d0fcfd33edaff868856"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_87.1.6%2Bg315d248%2Bchromium-87.0.4280.66_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="f4db8498f6d5799512277359ebae62b0f7930a52"
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
