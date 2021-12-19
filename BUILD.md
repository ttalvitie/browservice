# Building Browservice

This file contains building instructions on Linux. For build instructions on Windows, see the `README.md` file in the `winbuild` directory.

## Installing dependencies

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

On the proxy server, download and extract the [latest release](https://github.com/ttalvitie/browservice/releases) of the Browservice source code. Before compiling Browservice, we still need to download and set up CEF. To help with this, convenience scripts are provided. To download a release build of CEF into `cef.tar.bz2`, run the following in the extracted Browservice release root directory to download and verify CEF:

```
./download_cef.sh
```

Then, extract CEF and build its DLL wrapper library:

```
./setup_cef.sh
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

If you want to build the AppImage yourself, clone the repository and use the build script `tools/build_appimage.sh` with two arguments: the architecture (`x86_64`, `i386`, `armhf`, `aarch64`) and the Git branch, commit or tag name you want to build. The build script builds the AppImage in a QEMU emulated machine and if successfuly, saves the built AppImage to the current directory.
