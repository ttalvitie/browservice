# Running Browservice on macOS

Since Browservice only supports Linux, we run it via Docker.

## Prerequisites

- Docker Desktop for Mac

## One-time PulseAudio setup (for audio support)

```bash
brew install pulseaudio

mkdir -p ~/.config/pulse
cat > ~/.config/pulse/default.pa << 'PULSE'
load-module module-coreaudio-detect
load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1;172.17.0.0/16 auth-anonymous=1
load-module module-native-protocol-unix
PULSE
```

## Quick start

```bash
./start.sh
```

This will:
- Start PulseAudio if not already running
- Pull and run the correct image for your Mac
- Connect audio to your Mac speakers
- Expose Browservice on port 5555

## Apple Silicon (M1/M2/M3/M4)

```bash
docker run -d \
  --name browservice \
  -p 5555:5555 \
  --shm-size=256m \
  --privileged \
  --add-host=host.docker.internal:host-gateway \
  -e PULSE_SERVER=tcp:host.docker.internal:4713 \
  ghcr.io/startergo/browservice:latest-arm64
```

## Intel Mac

```bash
docker run -d \
  --name browservice \
  -p 5555:5555 \
  --shm-size=256m \
  --privileged \
  --add-host=host.docker.internal:host-gateway \
  -e PULSE_SERVER=tcp:host.docker.internal:4713 \
  ghcr.io/startergo/browservice:latest-amd64
```

## Connect from Win98/Snow Leopard in QEMU

In IE/Safari go to: `http://10.0.2.2:5555`

## Advanced options

```bash
/browservice/AppRun \
  --vice-opt-http-listen-addr=0.0.0.0:5555 \
  --vice-opt-default-quality=75 \
  --vice-opt-quality-selector=YES \
  --chromium-args=no-sandbox
```

## Build locally instead of pulling

```bash
# Apple Silicon
docker build -f Dockerfile.mac -t browservice-mac .

# Intel
docker build -f Dockerfile.intel -t browservice-intel .
```

## Notes

- dbus errors in logs are harmless
- `--privileged` is required for Chromium sandbox
- `--shm-size=256m` is required for Chromium shared memory
- `PULSE_SERVER` connects audio to your Mac speakers via PulseAudio TCP
- Images are auto-rebuilt daily when new Browservice releases are detected
- Proprietary codecs (H264/AAC) are not included — see BUILD.md to enable them

## Recommended: Native macOS build (Apple Silicon)

For best performance, build natively instead of using Docker:
- 60fps WebGL Aquarium at ~27% CPU and 10-15% GPU
- Native CoreAudio audio (no PulseAudio needed)
- Direct Metal GPU access

See [startergo/browservice-macos](https://github.com/startergo/browservice-macos/blob/master/MACOS_BUILD.md) for build instructions.

The Docker approach above is recommended for Intel Macs or when a native build is not needed.
