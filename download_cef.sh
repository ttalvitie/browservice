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
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.7%2Bg19ba721%2Bchromium-90.0.4430.212_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="f449aac4e0ce2d0a3f6da0005a0ade010b83103c"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.7%2Bg19ba721%2Bchromium-90.0.4430.212_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="7d90fed73ac42020973363ad924fd6fbad7a524b"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.7%2Bg19ba721%2Bchromium-90.0.4430.212_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="4c00009546821772ea6fe9a96b99f3843a9edf30"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_90.6.7%2Bg19ba721%2Bchromium-90.0.4430.212_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="828c1aad32eecfecccc120e1f0cebf629891eb05"
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
