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
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.4%2Bgbabcf94%2Bchromium-83.0.4103.106_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="c4d352ea3b14481d532bd9660a91425e8ccd9a29"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.4%2Bgbabcf94%2Bchromium-83.0.4103.106_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="2496e61173853cc712f55c7c85bd7c03a5e43639"
elif [ "$ARCH" == "armv7l" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.4%2Bgbabcf94%2Bchromium-83.0.4103.106_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="e1ecaa1681a1fe4b6d2a896cea49caed12d05a82"
elif [ "$ARCH" == "aarch64" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_83.4.4%2Bgbabcf94%2Bchromium-83.0.4103.106_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="0c1e83e0221742141d9e97a67d1c57f430813fdd"
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
