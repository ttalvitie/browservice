# Running Browservice on macOS (Apple Silicon)

Since Browservice only supports Linux, we run it via Docker using the aarch64 AppImage.

## Prerequisites

- Docker Desktop for Mac (Apple Silicon)
- The aarch64 AppImage from the releases page

## Setup

### 1. Download the AppImage

```bash
wget https://github.com/ttalvitie/browservice/releases/download/v0.9.12.2/browservice-v0.9.12.2-aarch64.AppImage
chmod +x browservice-v0.9.12.2-aarch64.AppImage
```

### 2. Build the Docker image

```bash
docker build -f Dockerfile.mac -t browservice-mac .
```

### 3. Run

```bash
docker run -d \
  --name browservice \
  -p 5555:5555 \
  --shm-size=256m \
  --privileged \
  browservice-mac \
  sh -c "Xvfb :99 -screen 0 1024x768x24 & DISPLAY=:99 /browservice/AppRun --vice-opt-http-listen-addr=0.0.0.0:5555 --chromium-args=no-sandbox"
```

### 4. Connect from Win98

In IE go to: `http://10.0.2.2:5555`

## Notes

- The dbus errors in logs are harmless
- `--privileged` is required for Chromium sandbox
- `--shm-size=256m` is required for Chromium shared memory
- Port 5555 can be changed as needed
