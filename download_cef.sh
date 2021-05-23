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
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.6%2Bg3c44b04%2Bchromium-90.0.4430.93_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="bddafa459df7a6b865b2066ac16c8e0f82b1cab9"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.6%2Bg3c44b04%2Bchromium-90.0.4430.93_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="c43df8549b701ffef7e3635ddab535e3eb696fd7"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.6%2Bg3c44b04%2Bchromium-90.0.4430.93_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="c9357f85c34c0daaa9cb731a251f45510fcf124b"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.6%2Bg3c44b04%2Bchromium-90.0.4430.93_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="2fb0ee0302f2c1e2304465def26d53d721b60db7"
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
