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
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.18%2Bgfe551e4%2Bchromium-96.0.4664.110_linux64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="5456140d047397c2ce7e2942b636123d67718c7c"
elif [ "$ARCH" == "armv7l" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.18%2Bgfe551e4%2Bchromium-96.0.4664.110_linuxarm.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="0638bff5abcda77ee2e7097a4236b3173bf5261b"
elif [ "$ARCH" == "aarch64" ]
then
    wget "https://cef-builds.spotifycdn.com/cef_binary_96.0.18%2Bgfe551e4%2Bchromium-96.0.4664.110_linuxarm64.tar.bz2" -O cef_unverified.tar.bz2
    SHA1="42f9769fe97b30cf41110e0ffdcccff3f516a8d0"
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
