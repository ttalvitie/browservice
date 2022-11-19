#!/bin/bash

set -e
set -o pipefail
shopt -s expand_aliases

msg() { cat <<< "--- $@" 1>&2; }

msg "Creating normal user for running the tests"
useradd -m user
alias U="sudo -u user"

msg "Copying AppImage and smoke test data to home directory"
cd /home/user
U cp /shared/browservice.AppImage browservice.AppImage
U cp /shared/smoke_test.py smoke_test.py
U cp -r /shared/smoke_test_data smoke_test_data
U chmod 700 browservice.AppImage

msg "Disabling man-db update to save time"
rm /var/lib/man-db/auto-update

msg "Disabling initramfs update to save time"
echo "update_initramfs=no" >> /etc/initramfs-tools/update-initramfs.conf

msg "Installing dependencies"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y python3-pip python3-numpy libjpeg-dev zlib1g-dev fuse libopenjp2-7
U pip install imageio

msg "Running smoke test"
U python3 smoke_test.py ./browservice.AppImage all
