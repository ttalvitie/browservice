# Building Browservice

This file contains building instructions for Linux. For building on Windows, see the `README.md` file in the `winbuild` directory.

## Building CEF

Browservice requires a custom build of CEF with some patches that make the embedded Chromium use an in-memory text-only clipboard that Browservice can access. It is recommended to build it using the Docker image [tools/linux_cef_build_docker_image/Dockerfile](tools/linux_cef_build_docker_image/Dockerfile) and the script [tools/build_patched_cef.py](tools/build_patched_cef.py).

Building CEF takes a lot of memory, disk space and time, because it includes the Chromium browser. You should build it on a powerful x86_64 Linux machine with Docker installed and at least 16 GB of memory and 100 GB of disk space. Note that even for ARM builds, you should use a x86_64 machine (CEF will be cross-compiled).

Instructions:
```
# Clone the Browservice code repository
git clone https://github.com/ttalvitie/browservice.git

# Create the cefbuild Docker image
cd browservice/tools/linux_cef_build_docker_image/
sudo docker build -t cefbuild .

# Prepare a build directory
cd ..
mkdir build
chmod 777 build
cp build_patched_cef.py build/

# Build CEF (this takes a lot of time; run in screen if you are behind an unreliable SSH connection)
# This command creates a x86_64 build; replace x86_64 by aarch64 for ARM64 and armhf for 32-bit ARM
# This will build a CEF version that is compatible with the Browservice code in the currently checked out commit
# (The version is overridable using the command line arguments of build_patched_cef.py)
sudo docker run -v "${PWD}/build":/home/appuser cefbuild python3 /home/appuser/build_patched_cef.py /home/appuser/build /home/appuser/patched_cef_x86_64.tar.bz2 x86_64

# Copy the built patched CEF distribution away from the build directory; we will need it when building Browservice
# (Again replace x86_64 by aarch64 or armhf if appropriate)
cp build/patched_cef_x86_64.tar.bz2 .

# Remove the build directory
sudo rm -r build
```

## Installing Browservice dependencies

### Ubuntu 18.04/20.04, Debian 10 and Raspberry Pi OS

```
sudo apt install wget cmake make g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 ttf-mscorefonts-installer xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0 libgtk-3-0
```

- On Debian, in order to be able to install the `ttf-mscorefonts-installer` package, you need to add the `contrib` APT source by adding `contrib` to the end of each `deb` and `deb-src` line in `/etc/apt/sources.list` and running `sudo apt update`.

- On Ubuntu, the installation of `ttf-mscorefonts-installer` often fails silently due to problems with the SourceForge mirrors. If the file `/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf` exists, your installation was successful. Otherwise, the texts in the Browservice UI will not work correctly. To rectify this, switch to the Debian package by running the following:

    ```
    sudo apt remove ttf-mscorefonts-installer
    wget https://www.nic.funet.fi/debian/pool/contrib/m/msttcorefonts/ttf-mscorefonts-installer_3.7_all.deb
    sudo dpkg -i ttf-mscorefonts-installer_3.7_all.deb
    ```

### Fedora 33

```
sudo dnf install wget tar bzip2 cmake make g++ pkg-config poco-devel libjpeg-turbo-devel pango-devel Xvfb xauth at-spi2-atk alsa-lib libXScrnSaver libXrandr libgbm libXcomposite libXcursor curl cabextract xorg-x11-font-utils nss cups-libs libatomic gtk3
sudo rpm -i https://downloads.sourceforge.net/project/mscorefonts2/rpms/msttcore-fonts-installer-2.6-1.noarch.rpm
```

### Arch Linux

```
sudo pacman -S wget cmake make gcc pkgconf poco pango libjpeg-turbo libxcb libx11 python xorg-server-xvfb xorg-xauth fakeroot at-spi2-atk alsa-lib nss libcups libxrandr libxcursor libxss libxcomposite libxkbcommon gtk3

# Install MS core fonts from AUR
wget https://aur.archlinux.org/cgit/aur.git/snapshot/ttf-ms-fonts.tar.gz
tar xf ttf-ms-fonts.tar.gz
pushd ttf-ms-fonts
makepkg -si
popd
rm -r ttf-ms-fonts ttf-ms-fonts.tar.gz
```

## Compiling and running Browservice

First build the patched CEF distribution as shown in the [Building CEF](#building-cef) section. Enter the Browservice working copy and run the following to extract the CEF distribution tarball and build its DLL wrapper library (replace `patched_cef_x86_64.tar.bz2` by the path of the CEF tarball you built):

```
./setup_cef.sh patched_cef_x86_64.tar.bz2
```

Now, compile a release build of Browservice (you may adjust the number in the `-j` argument to set the number of parallel compile jobs):

```
make -j5 release
```

After the build has completed, it will ask you to set the SUID permissions for the `chrome-sandbox` binary to enable the sandbox features of Chromium:

```
sudo chown root:root release/bin/chrome-sandbox && sudo chmod 4755 release/bin/chrome-sandbox
```

Now you can start the Browservice proxy:

```
release/bin/browservice
```

To run Browservice, you only need the contents of the `release/bin` directory; if you need to save disk space, you can delete the rest of the files.

If you want a debug build instead, replace `release` by `debug` in the commands.

For more information on how to use the Browservice proxy, refer to the instructions in README.md. The built executable can be used mostly in the same way as the prebuilt AppImage; only the automatic Verdana installation flag `--install-verdana` and the AppImage-specific flags such as `--appimage-extract` do not work.

## Building AppImage

If you want to build the AppImage yourself, clone the repository and use the build script `tools/build_appimage.sh` with four arguments: the architecture (`x86_64`, `armhf`, `aarch64`), the Git branch, commit or tag name you want to build, a path to the patched CEF tarball and the output filename. The build script builds the AppImage in a QEMU emulated machine and if successfuly, saves the built AppImage to the current directory.
