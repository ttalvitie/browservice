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
    wget "https://cef-builds.spotifycdn.com/cef_binary_88.2.6%2Bgd717f0e%2Bchromium-88.0.4324.150_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="832003033eff69bdcba3b4a56c4204145073dff2"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_88.2.6%2Bgd717f0e%2Bchromium-88.0.4324.150_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="78b5861f10a3737522e806c222af3f37331e5aeb"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_88.2.6%2Bgd717f0e%2Bchromium-88.0.4324.150_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="becaa089d44c7a0f575a8fcb360decc15df9d705"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_88.2.6%2Bgd717f0e%2Bchromium-88.0.4324.150_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="ffcdb9c750033bcba115cf0ec4a6bad363979d4d"
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
