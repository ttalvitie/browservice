#!/bin/bash -x

chromium_version=133.0.6943.142

# Avoid getting stuck in e.g. keyboard layout setting dialog.
export DEBIAN_FRONTEND=noninteractive

# Script from
# https://magpcss.org/ceforum/viewtopic.php?f=7&t=17776&p=46448#p54107
# linked from official automated build setup documentation in
# https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md

# Packages required to download and run the Chromium bootstrap.
apt-get update
apt-get install -y \
    curl \
    file \
    lsb-release \
    python3

# Download the Chromium bootstrap at the requested version.
curl "https://chromium.googlesource.com/chromium/src/+/refs/tags/${chromium_version}/build/install-build-deps.py?format=TEXT" | base64 -d > install-build-deps.py

# The install script expects sudo, so make a placeholder that transparently forwards commands.
if type sudo 2>/dev/null; then
 echo "The sudo command already exists... Skipping.";
else
 echo -e "#!/bin/bash\n\${@}" > /usr/sbin/sudo;
 chmod +x /usr/sbin/sudo;
fi

# Install with 32-bit and ARM dependencies included.
# NOTE: Installing locales fails, but we ignore the error.
python3 ./install-build-deps.py --lib32 --arm --no-chromeos-fonts --no-nacl --no-prompt

# Install pgrep which is used by depot_tools/.cipd_bin/goma_ctl.py.
apt-get install -y procps

# Install python dependencies required by Chromium.
apt-get install -y python3-pip
python3 -m pip install \
    dataclasses \
    importlib_metadata

# Install Doxygen which is used to generate CEF API documentation.
apt-get install -y doxygen

# Cleanup to reduce image size.
apt-get purge -y --auto-remove
