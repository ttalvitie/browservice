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
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.17%2Bg20e2617%2Bchromium-96.0.4664.93_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="2a5ba45ff7479781309905ba8b4d612243712a45"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.17%2Bg20e2617%2Bchromium-96.0.4664.93_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="f99ec503abb0e6acd0efb16ecc2f7949ccf24717"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.17%2Bg20e2617%2Bchromium-96.0.4664.93_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="67f93610b8e3d82b0bc0709cc8a269f9d881da8a"
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
