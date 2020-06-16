# Browservice: Browser as a Service
Browse the modern Web with old browsers using a proxy that runs the Chromium browser and streams the browser window as images

## Supported client browsers

The following OS-browser-combinations have been tested to work (some with limitations, see below the table for descriptions).

| Operating system            | Browser                    | Limitations |
| --------------------------- | -------------------------- | ----------- |
| Windows for Workgroups 3.11 | Internet Explorer 4        | [1] [2]     |
| Windows for Workgroups 3.11 | Internet Explorer 5        | [2]         |
| OS/2 Warp 4.52              | Firefox 2.0.0.14           |             |
| Windows 95                  | Internet Explorer 4.01 SP2 | [1] [2]     |
| Windows 95                  | Internet Explorer 5.5 SP2  |             |
| Windows 95                  | Firefox 1.5.0.12           |             |
| Windows 95                  | Opera 10.63                | [1] [3]     |
| Windows 98 Second Edition   | Internet Explorer 5        |             |
| Windows 98 Second Edition   | Internet Explorer 6 SP1    |             |
| Windows 98 Second Edition   | Firefox 2.0.0.20           |             |
| Windows 98 Second Edition   | Netscape 7.2               | [3]         |

[1] The mouse cursor flickers between the true cursor and an hourglass.

[2] Right click works, but it also opens a context menu in the client browser.

[3] The client browser back/forward buttons do not work (you can still use Backspace and Shift+Backspace).

## Setup

### Installing dependencies

The commands for installing dependencies on various Linux distributions are provided below. If the command for your distribution is missing, you may need to adapt the list and add missing packages through trial and error until the CEF DLL wrapper and Browservice compiles successfully.

#### Ubuntu 18.04/20.04 and Debian 10

```
sudo apt install cmake g++ pkg-config libxcb1-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 ttf-mscorefonts-installer xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1
```

- On Debian, in order to be able to install the `ttf-mscorefonts-installer` package, you need to add the `contrib` APT source by adding `contrib` to the end of each `deb` and `deb-src` line in `/etc/apt/sources.list` and running `sudo apt update`.

- On Ubuntu, the installation of `ttf-mscorefonts-installer` often fails silently due to problems with the SourceForge mirrors. If the file `/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf` exists, your installation was successful. Otherwise, the texts in the Browservice UI will not work correctly. To rectify this, switch to the Debian package by running the following:

    ```
    sudo apt remove ttf-mscorefonts-installer
    wget https://www.nic.funet.fi/debian/pool/contrib/m/msttcorefonts/ttf-mscorefonts-installer_3.7_all.deb
    sudo dpkg -i ttf-mscorefonts-installer_3.7_all.deb
    ```

#### Fedora 32

```
sudo dnf install cmake make g++ pkg-config poco-devel libjpeg-turbo-devel pango-devel Xvfb xauth at-spi2-atk alsa-lib libXScrnSaver libXrandr libgbm libXcomposite libXcursor curl cabextract xorg-x11-font-utils
sudo rpm -i https://downloads.sourceforge.net/project/mscorefonts2/rpms/msttcore-fonts-installer-2.6-1.noarch.rpm
```

#### Arch Linux

```
sudo pacman -S wget cmake make gcc pkgconf poco pango libjpeg-turbo libxcb python xorg-server-xvfb xorg-xauth fakeroot at-spi2-atk alsa-lib nss libcups libxrandr libxcursor libxss libxcomposite

# Install MS core fonts from AUR
wget https://aur.archlinux.org/cgit/aur.git/snapshot/ttf-ms-fonts.tar.gz
tar xf ttf-ms-fonts.tar.gz
pushd ttf-ms-fonts
makepkg -si
popd
rm -r ttf-ms-fonts ttf-ms-fonts.tar.gz
```

### Installing CEF

Obtain a release build of CEF (Chromium Embedded Framework) by running the following in this directory:

```
./download_cef.sh
```

Extract CEF and build its DLL wrapper library through which Browservice uses CEF:

```
./setup_cef.sh
```

### Compiling and running Browservice

Run a release build (you may adjust the number in the `-j` argument to set the number of parallel compile jobs):

```
make -j5
```

The build will ask you to set the SUID permissions for `chrome-sandbox`:

```
sudo chown root:root release/bin/chrome-sandbox && sudo chmod 4755 release/bin/chrome-sandbox
```

Now you are ready to run Browservice:

```
release/bin/browservice
```

With the default arguments, Browservice listens for local HTTP connections on port 8080. To stop the server, you can use the `SIGTERM` or `SIGINT` signals (you can send the latter using Ctrl+C).

By default, the listening socket is bound to `127.0.0.1`, which means that the server only accepts local connections. To allow remote computers to connect to the server, you need to adjust the `--http-listen-addr` command line argument; for example, to accept connections on all interfaces, bind to `0.0.0.0` as follows:

```
release/bin/browservice --http-listen-addr=0.0.0.0:8080
```

WARNING: Note that binding to `0.0.0.0` may allow unauthorized users to connect to the server. To avoid this, use a more restrictive listen address and/or a firewall.

There many are other useful command line options in Browservice. To get a list of them, run:

```
release/bin/browservice --help
```
