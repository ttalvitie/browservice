# Browservice: Browser as a Service

A web proxy server that makes it possible to browse the modern web on historical browsers. It works by rendering the browser viewport into images; the client browser shows these images using JavaScript and forwards input events back to the proxy.

![Screenshot of IE6 showing an Instagram page through Browservice](fig/nt4_ie6_instagram.png)

## How does it work?

The Browservice server uses [CEF (Chromium Embedded Framework)](https://bitbucket.org/chromiumembedded/cef) to run a Chromium browser instance that renders the browser view into an off-screen buffer. The browser view, combined with a control UI bar, is then compressed as a PNG or JPEG image and served to the client using an embedded HTTP server. The client browser runs a JavaScript application that requests and shows the images. It also listens for keyboard and mouse events from the user and forwards them to the proxy by including them in the URLs of the image requests.

Initially, this approach of sending the whole browser view as a new image every time it changes might sound quite inefficient. However, it is surprisingly usable if the network connection between the proxy server and the client is fast (such as 100 Mbit/s Ethernet LAN). Early 00s hardware (~1 GHz CPU clock) can often surpass 10 FPS when watching video. The performance is also tolerable on older machines if the JPEG compression level is set low and the browser window is small.

The current features of Browservice include the following:

- Viewing of and keyboard/mouse interaction with all the web pages supported by the Chromium browser; this includes web apps such as YouTube, Gmail, GitHub, Office on the web, Twitter, Facebook and Instagram
- Support for multiple concurrent browser windows
- Text clipboard common to all browser windows (accessed through Ctrl+C and Ctrl+V)
- Form for accessing the browser clipboard from the client side
- Control bar with an artisanal UI drawn in a style that blends well into a Windows 9x/NT4/IE6 environment
- Address field implemented entirely on the proxy server
- File downloads (with confirmation button on the control bar for security)
- Text search within the current page
- Image compression quality selectable on the fly (JPEG compression levels or PNG)
- Custom multithreaded implementation of PNG compression (standalone library; you just need `src/png.hpp` and `src/png.cpp`)

The following features are currently missing but could be implemented in future versions:

- Streaming audio to client (currently audio is played locally by the proxy server)
- Integration with web search engines
- Bookmarks
- File uploads

## Background

In retrocomputing, the modern web is inaccessible, because up-to-date web browsers are not available for old operating systems, and old browsers do not support modern web standards. Furthermore, old operating systems and browsers should not be connected directly to the Internet as they typically have unpatched security vulnerabilities. Browservice circumvents these issues by offloading the web rendering to a proxy server running an up-to-date web browser; the actual client browser connecting to the proxy only needs to show the images sent by the proxy server and forward the user input back to the proxy.

This idea of using a proxy to render the browser view into images has been used before by [WRP (Web Rendering proxy)](https://github.com/tenox7/wrp). Browservice differs from WRP in that it uses JavaScript on the client browser to animate the browser view and gather user input events, while in WRP, the user has to use web forms and image maps to provide the input, and the page has to be reloaded for every update in the view. Thus Browservice gives the user a more immersive web browsing experience, but also requires a newer client browser and more powerful hardware. While WRP can run on browsers as old as NCSA Mosaic 2.0, the [earliest supported client browsers](#supported-client-browsers) for Browservice are from late 90s and early 00s.

### Security

It is always a security risk to work with untrusted data (such as web page content) with outdated software. If we assume that the proxy server is kept up to date, Browservice significantly reduces the attack surface, because the untrusted data reaches the client only as images that were compressed by Browservice or file downloads or clipboard data that were explicitly requested by the user. Despite the reduced attack surface, security is not guaranteed, and you should make sure to really know what you are doing before using Browservice for security-critical web browsing.

## Supported client browsers

The following historical OS-browser-combinations have been confirmed to work as clients for the proxy (some with minor limitations; see the descriptions for the numbers below the table). In addition, modern browsers can typically be used as clients.

| Operating system             | Browser                    | Limitations |
| ---------------------------- | -------------------------- | ----------- |
| Windows for Workgroups 3.11  | Internet Explorer 4.0      | 1, 2, 3, 4  |
| Windows for Workgroups 3.11  | Internet Explorer 5        | 1, 3, 4     |
| OS/2 Warp 4.52               | Firefox 2.0.0.14           |             |
| Windows 95                   | Internet Explorer 4.01 SP2 | 2, 3, 4     |
| Windows 95                   | Internet Explorer 5.5 SP2  |             |
| Windows 95                   | Firefox 1.5.0.12           |             |
| Windows 95                   | Opera 10.63                | 2, 5        |
| Windows 98 Second Edition    | Internet Explorer 5        |             |
| Windows 98 Second Edition    | Internet Explorer 6 SP1    |             |
| Windows 98 Second Edition    | Firefox 2.0.0.20           |             |
| Windows 98 Second Edition    | Netscape 7.2               | 6           |
| Windows 98 Second Edition    | Netscape 9.0.0.6           |             |
| Windows NT 4.0 SP6a          | Internet Explorer 4.0      | 2, 3, 4, 7  |
| Windows NT 4.0 SP6a          | Internet Explorer 5        |             |
| Windows NT 4.0 SP6a          | Internet Explorer 5.5 SP2  |             |
| Windows NT 4.0 SP6a          | Internet Explorer 6 SP1    |             |
| Windows NT 4.0 SP6a          | Firefox 1.0.1              | 6           |
| Windows NT 4.0 SP6a          | Firefox 2.0.0.20           |             |
| Windows NT 4.0 SP6a          | Netscape 9.0.0.6           |             |
| Windows XP SP3               | Internet Explorer 6 SP3    |             |
| Windows XP SP3               | Internet Explorer 8        |             |
| Windows XP SP3               | Firefox 1.0.1              | 6           |
| Windows XP SP3               | Firefox 52.3.0 ESR         |             |
| Windows XP SP3               | Chrome 1.0.154             |             |
| Windows XP SP3               | Chrome 49.0.2623           |             |
| Debian GNU/Linux 3.1 "Sarge" | Firefox 1.0.4              |             |

1. PNG is not supported.
2. The mouse cursor flickers between the true cursor and an hourglass.
3. Right click works, but it also opens a context menu in the client browser.
4. The browser has the following bug that affects Browservice: In some cases where the browser window loses focus (such as when pressing the Ctrl+F key), the keyboard handler may move into an invalid state where some keys are unavailable or the Ctrl key is stuck down. To rectify this, the user has to manually press and release Ctrl.
5. The client browser back/forward buttons do not work (you can still use Backspace and Shift+Backspace).
6. Typing special characters using the AltGr key does not work in the browser area (you can still paste them from the clipboard).
7. The names of downloaded files are garbled.

The client support has been tested using the procedure of the `test` directory. It would be interesting to hear about your experiences on different platforms. Browservice is expected to work on Internet Explorer or Firefox on old m68k and PowerPC Macs, but due to lack of access, these have not been tested.

## Setup

A Browservice setup consists of two machines; the Browservice proxy server and the client. Currently, Linux is the only supported operating system for the proxy server; the supported CPU architectures are i386, x86_64, ARM and ARM64. The proxy server should also have sufficient memory and CPU performance to run the Chromium browser. For the client, many different operating systems and browsers should work, but the greatest chance of success is achieved using an OS-browser-combination that is close to one of the entries in the [table of supported client browsers](#supported-client-browsers).

Only the proxy server needs an Internet connection; the client only needs to be able to connect to the proxy server. It is advisable not to expose the client directly to the Internet. One possible network setup is to have two interfaces on the proxy server: one for Internet and another for the local connection to the client. If both machines are virtual machines, host-only adapters can be used for the local connection between the proxy server and the client.

Here is one example of a mobile Browservice setup: The proxy server is running on a Raspberry Pi 4 (fitted with a fan and a battery) that connects to the Internet using Wi-Fi. The client is an IBM ThinkPad T40 running Windows NT 4.0 and Internet Explorer 6. The client connects to the proxy server through a direct patch cable between the Ethernet ports of the machines; the Ethernet interfaces are configured with static IPs in the same private subnet.

TODO: picture of the setup

### Installing dependencies

The commands for installing the dependencies of the Browservice proxy on various Linux distributions are provided in this section.

#### Ubuntu 18.04/20.04, Debian 10 and Raspberry Pi OS

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

### Compiling Browservice

Download and extract the [latest release](https://github.com/ttalvitie/browservice/releases) of the Browservice source code. Before compiling Browservice, we still need to download and set up CEF. To help with this, we provide two scripts. To download a release build of CEF, run the following in the extracted Browservice release root directory:

```
./download_cef.sh
```

Then, extract CEF and build its DLL wrapper library:

```
./setup_cef.sh
```

Now, compile a release build of Browservice (you may adjust the number in the `-j` argument to set the number of parallel compile jobs):

```
make -j5
```

After the build has completed, it will ask you to set the SUID permissions for `chrome-sandbox` to enable some sandbox features of Chromium:

```
sudo chown root:root release/bin/chrome-sandbox && sudo chmod 4755 release/bin/chrome-sandbox
```

Now you can run Browservice:

```
release/bin/browservice
```

With the default arguments, Browservice listens for HTTP connections on port 8080. To stop the server, you can use the `SIGTERM` or `SIGINT` signals (you can send the latter using Ctrl+C).

By default, the listening socket is bound to `127.0.0.1`, which means that the server only accepts local connections. To allow remote computers to connect to the server, you need to adjust the `--http-listen-addr` command line argument; for example, to accept connections on all interfaces, bind to `0.0.0.0` as follows:

```
release/bin/browservice --http-listen-addr=0.0.0.0:8080
```

WARNING: Note that binding to `0.0.0.0` may allow unauthorized users to connect to the server. To avoid this, use a more restrictive listen address and/or a firewall.

There many are other useful command line options in Browservice. To get a list of them, run:

```
release/bin/browservice --help
```
