# Browservice: Browser as a Service

A web "proxy" server that enables browsing the modern web on historical browsers. It works by rendering the browser viewport into images, which are then shown by a JavaScript application running on the client browser.

![Screenshot of IE6 showing an Instagram page through Browservice](fig/nt4_ie6_instagram.png)

[<img src="fig/youtube_gmail_demo.png" alt="YouTube video of Gmail usage on IE6 with the help of Browservice" width="650">](https://www.youtube.com/watch?v=oI6wJbMKjoQ)

[See more screenshots](#screenshots)

## How does it work?

The Browservice server uses [CEF (Chromium Embedded Framework)](https://bitbucket.org/chromiumembedded/cef) to run a Chromium browser instance that renders the browser view into an off-screen buffer. The browser view, combined with a control UI bar, is then compressed as a PNG or JPEG image and served to the client using an embedded HTTP server. The client browser runs a JavaScript application that requests and shows the images. It also listens for keyboard and mouse events from the user and forwards them to the proxy by including them in the URLs of the image requests.

Initially, this approach of sending the whole browser view as a new image every time it changes might sound quite inefficient. However, it is surprisingly usable if the network connection between the proxy server and the client is fast (such as 100 Mbit/s Ethernet LAN). Early 00s hardware (~1 GHz CPU clock) can often surpass 10 FPS in video streaming. The performance is also tolerable on older machines if a low JPEG compression level is used and the browser window is small.

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
- Native Back/Forward/Refresh buttons on the client forwarded to the browser
- Custom multithreaded implementation of PNG compression (standalone library; you just need `src/png.hpp` and `src/png.cpp`)

The following features are currently missing but could be implemented in future versions:

- Streaming audio to client (currently audio is played locally by the proxy server)
- Less hackish keyboard handling (different browsers send very different JavaScript key events)
- Integration with web search engines
- Bookmarks
- File uploads
- Page zooming

## Background

In retrocomputing, the modern web is inaccessible, because up-to-date web browsers are not available for old operating systems, and old browsers do not support modern web standards. Furthermore, old operating systems and browsers should not be connected directly to the Internet because they typically have unpatched security vulnerabilities. Browservice circumvents these issues by offloading the web rendering to a proxy server running an up-to-date web browser; the actual client browser connecting to the proxy only needs to show the images sent by the proxy server and forward the user input back to the proxy.

This idea of using a proxy to render the browser view into images has been used before by [WRP (Web Rendering proxy)](https://github.com/tenox7/wrp). Browservice differs from WRP in that it uses JavaScript on the client browser to animate the browser view and gather user input events, while in WRP, the user has to use web forms and image maps to provide the input, and the page has to be reloaded for every update in the view. Thus Browservice gives the user a more immersive web browsing experience, but also requires a newer client browser and more powerful hardware. While WRP can run on browsers as old as NCSA Mosaic 2.0, the [earliest supported client browsers](#supported-client-browsers) for Browservice are from late 90s and early 00s.

### Security

It is always a security risk to work with untrusted data (such as web page content) with outdated software. If we assume that the proxy server is kept up to date, and the connection between the proxy server and the client is made through a trusted isolated network, Browservice significantly reduces the attack surface, because the untrusted data reaches the client only as images that were compressed by Browservice or file downloads or clipboard data that were explicitly requested by the user. Not even the URLs of the accessed pages are sent to the client browser as text because the address bar is rendered on the server side (Browservice is not a real HTTP proxy). Despite the reduced attack surface, security is not guaranteed, and you should make sure to really know what you are doing before using Browservice for security-critical web browsing.

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

The client support has been tested using the procedure documented in the `test` directory. It would be interesting to hear about your experiences on different platforms. Browservice is expected to work on Internet Explorer or Firefox on old m68k and PowerPC Macs, but due to lack of access, these have not been tested.

## Setup

A Browservice setup consists of two machines; the Browservice proxy server and the client. Currently, Linux is the only supported operating system for the proxy server; the supported CPU architectures are i386, x86_64, ARM and ARM64. The proxy server should also have sufficient memory and CPU performance to run the Chromium browser. Due to the Linux kernel features required by the Chromium security sandbox, it is difficult to get the Browservice proxy to run in a Docker container; you should use a Linux virtual machine instead (or WSL 2 on Windows).

For the client, many different operating systems and browsers should work, but the greatest chance of success is achieved using an OS-browser-combination that is close to one of the entries in the [table of supported client browsers](#supported-client-browsers). The performance of the different client browsers have not been benchmarked, but as a rule of thumb for Windows systems, the newest version of Internet Explorer available for the system is typically the fastest.

Only the proxy server needs an Internet connection; the client only needs to be able to connect to the proxy server. It is advisable not to expose the client directly to the Internet. One possible network setup is to have two interfaces on the proxy server: one for Internet and another for the local connection to the client. If both machines are virtual machines, host-only adapters can be used for the local connection between the proxy server and the client. The proxy server should be configured to only accept connections from the isolated local network between the proxy and client machines to prevent unauthorized users from gaining access to the browser.

Here is one example of a mobile hardware Browservice setup that has shown to be working well: The proxy server is running on a Raspberry Pi 4 (fitted with a fan and a battery) that connects to the Internet using Wi-Fi. The client is an IBM ThinkPad T40 running Windows NT 4.0 and Internet Explorer 6. The client connects to the proxy server through a direct patch cable between the Ethernet ports of the machines; the Ethernet interfaces are configured with static IPs in the same private subnet.

![Photo of ThinkPad T40 showing a GitHub page, and a Raspberry Pi 4 running the Browservice proxy connected to the laptop with an Ethernet patch cable](fig/raspi4_t40.jpg)

### Installing dependencies

The commands for installing the dependencies of the Browservice proxy on various Linux distributions are provided in this section.

#### Ubuntu 18.04/20.04, Debian 10 and Raspberry Pi OS

```
sudo apt install wget cmake g++ pkg-config libxcb1-dev libx11-dev libpoco-dev libjpeg-dev zlib1g-dev libpango1.0-dev libpangoft2-1.0-0 ttf-mscorefonts-installer xvfb xauth libatk-bridge2.0-0 libasound2 libgbm1 libxi6 libcups2 libnss3 libxcursor1 libxrandr2 libxcomposite1 libxss1 libxkbcommon0
```

- On Debian, in order to be able to install the `ttf-mscorefonts-installer` package, you need to add the `contrib` APT source by adding `contrib` to the end of each `deb` and `deb-src` line in `/etc/apt/sources.list` and running `sudo apt update`.

- On Ubuntu, the installation of `ttf-mscorefonts-installer` often fails silently due to problems with the SourceForge mirrors. If the file `/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf` exists, your installation was successful. Otherwise, the texts in the Browservice UI will not work correctly. To rectify this, switch to the Debian package by running the following:

    ```
    sudo apt remove ttf-mscorefonts-installer
    wget https://www.nic.funet.fi/debian/pool/contrib/m/msttcorefonts/ttf-mscorefonts-installer_3.7_all.deb
    sudo dpkg -i ttf-mscorefonts-installer_3.7_all.deb
    ```

#### Fedora 33

```
sudo dnf install wget tar bzip2 cmake make g++ pkg-config poco-devel libjpeg-turbo-devel pango-devel Xvfb xauth at-spi2-atk alsa-lib libXScrnSaver libXrandr libgbm libXcomposite libXcursor curl cabextract xorg-x11-font-utils nss cups-libs
sudo rpm -i https://downloads.sourceforge.net/project/mscorefonts2/rpms/msttcore-fonts-installer-2.6-1.noarch.rpm
```

#### Arch Linux

```
sudo pacman -S wget cmake make gcc pkgconf poco pango libjpeg-turbo libxcb libx11 python xorg-server-xvfb xorg-xauth fakeroot at-spi2-atk alsa-lib nss libcups libxrandr libxcursor libxss libxcomposite libxkbcommon

# Install MS core fonts from AUR
wget https://aur.archlinux.org/cgit/aur.git/snapshot/ttf-ms-fonts.tar.gz
tar xf ttf-ms-fonts.tar.gz
pushd ttf-ms-fonts
makepkg -si
popd
rm -r ttf-ms-fonts ttf-ms-fonts.tar.gz
```

### Compiling and running Browservice

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
make -j5
```

After the build has completed, it will ask you to set the SUID permissions for the `chrome-sandbox` binary to enable the sandbox features of Chromium:

```
sudo chown root:root release/bin/chrome-sandbox && sudo chmod 4755 release/bin/chrome-sandbox
```

Now you can start the Browservice proxy:

```
release/bin/browservice
```

With the default arguments, the Browservice proxy listens for HTTP connections on port 8080. To stop the server, you can use the `SIGTERM` or `SIGINT` signals (you can send the latter using Ctrl+C).

By default, the listening socket is bound to `127.0.0.1`, which means that the server only accepts local connections. To allow other computers to connect to the server, you need to adjust the `--vice-opt-http-listen-addr` command line option; for example, to accept connections on all interfaces, bind to `0.0.0.0` as follows:

```
# See WARNINGs below!
release/bin/browservice --vice-opt-http-listen-addr=0.0.0.0:8080
```

**WARNING**: Binding to `0.0.0.0` may allow unauthorized users to connect to the server. Giving untrusted users access to the server is very dangerous; for example, they can access all the user accounts on websites to which you have logged in using Browservice. To avoid this, restrict the incoming connections to isolated local networks using a restrictive listen address and/or a firewall.

**WARNING**: Browservice does not support encrypted connections between the Browservice proxy and the client browser. Therefore, these connections should always be made over a completely trusted/isolated network (or over a secure tunnel).

**WARNING**: The trust between the client and the proxy server has to be mutual, as the client controls a web browser process running on the proxy server. For example, the client can use the `file://` protocol to read files on the proxy server that are accessible to the user running `browservice`.

**WARNING**: The embedded Chromium browser does not update itself. To keep the browser up to date, you should periodically install the newest release of Browservice; on each release, the `download_cef.sh` script is updated to use the newest CEF release.

The clipboard and browser storage (cookies, local storage, cache, etc.) are shared among all the clients of the same Browservice instance, and thus you should start a separate instance for each user. By default, the browser runs in incognito mode, which means that all the browser storage is lost when the Browservice server is stopped. To avoid losing your session cookies and cache, you can persist the storage by specifying an absolute path to the storage directory in the `--data-dir` option (for example `--data-dir=$HOME/.browservice`)

There are many other useful command line options in Browservice. To get a list of them, run:

```
release/bin/browservice --help
```

To run Browservice, you only need the contents of the `release/bin` directory; if you need to save disk space, you can delete the rest of the files.

## Usage

To open a new browser window, you should navigate the client browser to the address where the Browservice proxy server is listening (for example, `http://192.168.56.1:8080/`). To make it easier to open new browser windows, this should be set as the home page for the client browser.

After this, you can browse the web as you typically would, as long as you remember to use the in-page Browservice "Address" text field instead of the native browser address field. In some client browsers, you can disable the native browser controls to free up screen space and avoid confusion.

Some notes that might be useful:

- If the page offers a file for download, you need to explicitly accept it by clicking the Download button that appears in the control bar (after making sure that you trust the page). Browservice then downloads the file into a temporary directory on the proxy server, after which it forwards the file to the client browser.

- If the page requests a file upload, a separate upload popup window is automatically opened. After uploading the file in the upload window, the file will be placed in a temporary directory and forwarded to the page requesting the upload. Due to technical limitations, all the uploaded files will be stored in the temporary directory until the browser window is closed.

- If the page opens a popup window, Browservice automatically opens a new client browser window for it. However, creating this window might be blocked by the popup blocker of the client browser, and you may need to explicitly allow it or change the blocker settings.

- You can use Ctrl+C and Ctrl+V to copy and paste text between different Browservice windows (including the control bar). However, this clipboard is not automatically shared with the client system. To access the clipboard from the client, click the clipboard icon in the control bar to open a window that contains a text field and buttons for loading the clipboard into the text field and saving the contents of the text field into the clipboard. Note that it is a security risk to load text copied from an untrusted page into the native text field.

- The most common browser hotkeys (Backspace, Shift+Backspace, Ctrl+F, Ctrl+L, Ctrl+A, Ctrl+R, F5, PgUp/PgDown, Home/End) work with Browservice. However, some client browsers capture these keys instead of sending them to Browservice. For some Ctrl-based shortcuts, you can get around this by adding Shift into the combination, for example using Ctrl+Shift+F instead of Ctrl+F.

- If you have many browser windows open at the same time, your may experience lag due to the per-server keep-alive connection limit of the client browser, as Browservice uses long polling HTTP requests. If you use Internet Explorer version up to 6 on Windows, the limit can be set by creating/setting the `MaxConnectionsPerServer` DWORD value in registry key `HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings`. As a rule of thumb, the value should be at least the number of browser windows multiplied by two.

- By default, Browservice can't play videos that use proprietary audio/video codecs such as H264 and AAC, as the prebuilt CEF distribution [provided by Spotify](http://opensource.spotify.com/cefbuilds/index.html) does not include them. To add the codecs, build the CEF distribution by following the [instructions](https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md) with the options `proprietary_codecs=true ffmpeg_branding=Chrome` appended to the environment variable `GN_DEFINES`. After this, you should repeat the Browservice installation process with one exception: prior to running `setup_cef.sh`, copy the CEF distribution produced by the build to `cef.tar.bz2` instead of using `download_cef.sh` to download it. Note that building CEF takes a lot of time, memory and disk space. Also note that you may have to pay license fees to use the proprietary codecs legally, as they are encumbered by patents.

## Screenshots

![Screenshot of Internet Explorer 6 on Windows NT 4.0 showing Hacker News](fig/nt4_ie6_hackernews.png)
Browsing Hacker News using Windows NT 4.0 and Internet Explorer 6.

![Screenshot of Internet Explorer 4.0 on Windows NT 4.0 uploading files to Google Drive](fig/nt4_ie4_driveupload.png)
Uploading files to Google Drive using Windows NT 4.0 and Internet Explorer 4.0.

![Screenshot of Internet Explorer 4.0 on Windows for Workgroups 3.11 showing Hacker News](fig/w311_ie4_cloud.png)
Managing Google Cloud virtual machines using Windows for Workgroups 3.11 and Internet Explorer 4.0.

![Screenshot of Internet Explorer 5 on Windows 98 SE showing a YouTube video](fig/w98_ie5_legend.png)
Watching the *Legend of Microsoft Windows 98* on YouTube using Windows 98 SE and Internet Explorer 5. The audio is played directly by the proxy server.

![Screenshot of Firefox 2 on OS/2 Warp 4.52 showing a Wikipedia page](fig/os2_firefox2_wikipedia.png)
Reading up on the history of OS/2 from Wikipedia using OS/2 Warp 4.52 and Firefox 2.0.0.14.

![Screenshot of Opera 10.63 on Windows 95 showing a driver download page](fig/w95_opera1063_drivers.png)
Downloading ThinkPad drivers using Windows 95 and Opera 10.63 with the help of the Find feature.

![Screenshot of Netscape 9 on Windows NT 4.0 running multiple nested Browservice browsers](fig/nt4_netscape9_recursive.png)
Using Browservice recursively on Windows NT 4.0 and Netscape Navigator 9.0.0.6.

> *Browservice is 100% AJAX-free software, handmade in Finland*
