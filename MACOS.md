# Running Browservice on macOS (Apple Silicon)

Since Browservice only supports Linux, we run it via Docker using the aarch64 AppImage.

## Prerequisites

- Docker Desktop for Mac (Apple Silicon)

## Setup

### 1. Build the Docker image

```bash
docker build -f Dockerfile.mac -t browservice-mac .
```

### 2. Run

```bash
docker run -d \
  --name browservice \
  -p 5555:5555 \
  --shm-size=256m \
  --privileged \
  browservice-mac
```

### 3. Connect from Win98

In IE go to: `http://10.0.2.2:5555`

## Notes

- The dbus errors in logs are harmless
- `--privileged` is required for Chromium sandbox
- `--shm-size=256m` is required for Chromium shared memory
- Port 5555 can be changed as needed
