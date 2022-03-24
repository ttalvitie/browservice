#!/bin/bash -x
# Based on https://magpcss.org/ceforum/viewtopic.php?f=7&t=17776&p=46448#p47371

chromium_version=99.0.4844.83
export DEBIAN_FRONTEND=noninteractive

# Packages required to download and run the Chromium bootstrap.
apt-get update
apt-get install -y \
    curl \
    lsb-release \
    python

# Download the Chromium bootstrap at the requested version.
curl "https://chromium.googlesource.com/chromium/src/+/refs/tags/${chromium_version}/build/install-build-deps.sh?format=TEXT" | base64 -d > install-build-deps.sh
chmod 755 install-build-deps.sh

# Remove the "Installing locales" step which fails with sed errors.
# TODO: This trims all the way to end-of-file. Fix the sed error or
# add a more nuanced approach in the future if necessary.
line_num="$(grep -n 'echo \"Installing locales.\"' install-build-deps.sh | head -n 1 | cut -d: -f1)"
sed -i "${line_num},\$ d" install-build-deps.sh

# The install script expects sudo, so make a placeholder that transparently forwards commands.
if type sudo 2>/dev/null; then
 echo "The sudo command already exists... Skipping.";
else
 echo -e "#!/bin/bash\n\${@}" > /usr/sbin/sudo;
 chmod +x /usr/sbin/sudo;
fi

# Install with i386 and ARM dependencies included.
./install-build-deps.sh --lib32 --arm --no-chromeos-fonts --no-nacl --no-prompt

# Fix the following error with i386 and ARM builds:
# ././v8_context_snapshot_generator: error while loading shared libraries: libglib-2.0.so.0: cannot open shared object file: No such file or directory
apt-get install -y libglib2.0-0:i386

# Cleanup packages to reduce image size.
apt-get purge -y --auto-remove
