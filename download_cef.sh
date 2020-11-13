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
    wget "https://cef-builds.spotifycdn.com/cef_binary_86.0.21%2Bg6a2c8e7%2Bchromium-86.0.4240.183_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="bb3ca59e5d27fc69af60d1da2819d7d79fd61133"
elif [[ "$ARCH" =~ ^i[3-7]86$ ]]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_84.4.1%2Bgfdc7504%2Bchromium-84.0.4147.105_linux32.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="a699923b00fb774f4018bad792b04d5b27ae9ce5"
elif [ "$ARCH" == "armv7l" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_84.4.1%2Bgfdc7504%2Bchromium-84.0.4147.105_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="727e67d246d5ec91fd417dfbbbd4b0d75976707a"
elif [ "$ARCH" == "aarch64" ]
then
    wget "http://opensource.spotify.com/cefbuilds/cef_binary_84.4.1%2Bgfdc7504%2Bchromium-84.0.4147.105_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="07bf5bcaea805af7809b3e05db243ad5879faae9"
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
