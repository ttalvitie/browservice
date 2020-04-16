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
    wget http://opensource.spotify.com/cefbuilds/cef_binary_81.2.16%2Bgdacda4f%2Bchromium-81.0.4044.92_linux64.tar.bz2 -O cef_unverified.tar.bz2
    SHA1="2f89836f064a11fcfa68214a9ffba7c953fc6188"
elif [ "$ARCH" == "armv7l" ]
then
    wget http://opensource.spotify.com/cefbuilds/cef_binary_81.2.16%2Bgdacda4f%2Bchromium-81.0.4044.92_linuxarm.tar.bz2 -O cef_unverified.tar.bz2
    SHA1="8cbeb66c6876c68ab92f9456b093964fde3d54b1"
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
