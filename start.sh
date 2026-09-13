#!/bin/bash
PORT=${1:-5555}
QUALITY=${2:-75}

# Start PulseAudio if not running
pulseaudio --check || pulseaudio --exit-idle-time=-1 --disable-shm=true --daemonize=yes

echo "Starting Browservice on port $PORT with quality $QUALITY..."

docker rm -f browservice 2>/dev/null

docker run -d \
  --name browservice \
  -p ${PORT}:5555 \
  --shm-size=256m \
  --privileged \
  --add-host=host.docker.internal:host-gateway \
  -e PULSE_SERVER=tcp:host.docker.internal:4713 \
  ghcr.io/startergo/browservice:latest-arm64 \
  /browservice/AppRun \
  --vice-opt-http-listen-addr=0.0.0.0:5555 \
  --vice-opt-default-quality=${QUALITY} \
  --vice-opt-quality-selector=YES \
  --chromium-args=no-sandbox

echo "Browservice running at http://localhost:${PORT}"
echo "From QEMU Win98/Snow Leopard: http://10.0.2.2:${PORT}"
