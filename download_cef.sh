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
    wget "https://cef-builds.spotifycdn.com/cef_binary_94.3.0%2Bgffef496%2Bchromium-94.0.4606.54_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="1b1d834449aa54ae8887d8056eb9a6b961c1c997"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_94.3.0%2Bgffef496%2Bchromium-94.0.4606.54_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="6972594641d4ee565e7ded3b913a4ce0030a3022"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_94.3.0%2Bgffef496%2Bchromium-94.0.4606.54_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="0c88d899874aa3811bc1b5b34cd91b398b8f4469"
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
