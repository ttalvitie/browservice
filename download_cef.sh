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
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.0%2Bgfd6631b%2Bchromium-83.0.4103.106_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="b1b7304989f63f82f0ffb1a9a12202dd516b99c5"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.0%2Bgfd6631b%2Bchromium-83.0.4103.106_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="f2371d1fa647e530767fc79df8a0d693726e2e0e"
elif [ "$ARCH" == "armv7l" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.0%2Bgfd6631b%2Bchromium-83.0.4103.106_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="00fa17ef61829e08cda26abbfbf465abf31fa85f"
elif [ "$ARCH" == "aarch64" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.0%2Bgfd6631b%2Bchromium-83.0.4103.106_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="6977bc727da5eac6256ec01e9b5e068a7bc04cfe"
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
