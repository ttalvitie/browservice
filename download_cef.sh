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
    wget "https://cef-builds.spotifycdn.com/cef_binary_104.4.18%2Bg2587cf2%2Bchromium-104.0.5112.81_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="9997d1ec8b3de0bd32736bfaa39aa140d1826e3b"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_104.4.18%2Bg2587cf2%2Bchromium-104.0.5112.81_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="bc0cfdaf43e6a213de58217ef92326ac567cf627"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_104.4.18%2Bg2587cf2%2Bchromium-104.0.5112.81_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="fc46b349984a33d2415a44849ba8b77fa244da6a"
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
