# BaaS: Browser as a Service
Browse the modern Web with old browsers using a proxy that runs the Chromium browser and streams the browser window as images

## Setup

Install the dependencies:

```
sudo apt install cmake g++ pkg-config libx11-dev libpoco-dev libjpeg-turbo8-dev zlib1g-dev libnss3 libxrandr2 libx11-xcb1 libxcomposite1 libpangocairo-1.0-0 libxcursor1 libxdamage1 libatk-bridge2.0-0 libasound2 libcups2 libxss1 libxi6 xvfb libgbm1 libpango1.0-dev libpangoft2-1.0-0 ttf-mscorefonts-installer
```

- On Ubuntu, the installation of `ttf-mscorefonts-installer` often fails silently due to problems with the SourceForge mirrors. If the file `/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf` exists, your installation was successful. Otherwise, the texts in the BaaS UI will not work correctly. To rectify this, switch to the Debian package by running the following

    ```
    sudo apt remove ttf-mscorefonts-installer
    wget https://www.nic.funet.fi/debian/pool/contrib/m/msttcorefonts/ttf-mscorefonts-installer_3.7_all.deb
    sudo dpkg -i ttf-mscorefonts-installer_3.7_all.deb
    ```

Obtain a release build of CEF (Chromium Embedded Framework):

```
./download_cef.sh
```

Extract CEF and build its DLL wrapper library that we will use:

```
./setup_cef.sh
```

Run a release build:

```
make -j5
```

The build will ask you to set the SUID permissions for `chrome-sandbox`:

```
sudo chown root:root release/bin/chrome-sandbox && sudo chmod 4755 release/bin/chrome-sandbox
```

You will need to have an X server running and the `DISPLAY` environment variable pointed to it. One way that works even on headless servers is to use Xvfb:

```
Xvfb :0 -screen 0 640x480x24 &
export DISPLAY=:0
```

Now you are ready to run BaaS:

```
release/bin/baas
```

The server is listening for HTTP connections on port 8080. To stop the server, you can use the `SIGTERM` or `SIGINT` signals (you can send the latter using Ctrl+C).
