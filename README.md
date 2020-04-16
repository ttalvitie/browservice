# BaaS: Browser as a Service
Browse the modern Web with old browsers using a proxy that runs the Chromium browser and streams the browser window as images

## Installation

Install the dependencies:

```
sudo apt install git cmake g++ pkg-config libx11-dev libpoco-dev libjpeg-turbo8-dev zlib1g-dev libnss3 libxrandr2 libx11-xcb1 libxcomposite1 libpangocairo-1.0-0 libxcursor1 libxdamage1 libatk-bridge2.0-0 libasound2 libcups2 libxss1 libxi6 xvfb libpango1.0-dev libpangoft2-1.0-0 ttf-mscorefonts-installer
```

Obtain a release build of CEF (Chromium Embedded Framework):

```
./download_cef.sh
```

TODO