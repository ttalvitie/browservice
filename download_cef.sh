#!/bin/bash

set -e

if [ -e ./cef.tar.bz2 ]
then
    echo "./cef.tar.bz2 already exists!"
    exit 1
fi

ARCH=`arch`

if [ "$ARCH" == "x86_64" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.3.12%2Bg0889ff0%2Bchromium-83.0.4103.97_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="40774d539d48ae3d314796cb4965ac0aae87c1b3"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.3.12%2Bg0889ff0%2Bchromium-83.0.4103.97_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="ac77f411faaaeb93077ed648b1d4ffa7412fa03d"
elif [ "$ARCH" == "armv7l" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.3.12%2Bg0889ff0%2Bchromium-83.0.4103.97_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="055b6d478c85d88969510d210ded2ec7dd9fd1f6"
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
