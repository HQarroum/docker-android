#!/bin/bash

set -e

source ./emulator-monitoring.sh

# The emulator console port.
EMULATOR_CONSOLE_PORT=5554
# The ADB port used to connect to ADB.
ADB_PORT=5555
OPT_MEMORY=${MEMORY:-8192}
OPT_CORES=${CORES:-4}
OPT_SKIP_AUTH=${SKIP_AUTH:-true}
AUTH_FLAG=
# Start ADB server by listening on all interfaces.
echo "Starting the ADB server ..."
adb -a -P 5037 server nodaemon &

# Detect ip and forward ADB ports from the container's network
# interface to localhost.
LOCAL_IP=$(ip addr list eth0 | grep "inet " | cut -d' ' -f6 | cut -d/ -f1)
socat tcp-listen:"$EMULATOR_CONSOLE_PORT",bind="$LOCAL_IP",fork tcp:127.0.0.1:"$EMULATOR_CONSOLE_PORT" &
socat tcp-listen:"$ADB_PORT",bind="$LOCAL_IP",fork tcp:127.0.0.1:"$ADB_PORT" &

export USER=root

# Creating the Android Virtual Emulator.
TEST_AVD=$(avdmanager list avd | grep -c "android.avd" || true)
if [ "$TEST_AVD" == "1" ]; then
  echo "Use the exists Android Virtual Emulator ..."
else
  echo "Creating the Android Virtual Emulator ..."
  echo "Using package '$PACKAGE_PATH', ABI '$ABI' and device '$DEVICE_ID' for creating the emulator"
  echo no | avdmanager create avd \
    --force \
    --name android \
    --abi "$ABI" \
    --package "$PACKAGE_PATH" \
    --device "$DEVICE_ID"
fi

if [ "$OPT_SKIP_AUTH" == "true" ]; then
  AUTH_FLAG="-skip-adb-auth"
fi

# If GPU acceleration is enabled, we create a virtual framebuffer
# to be used by the emulator when running with GPU acceleration.
# We also set the GPU mode to `host` to force the emulator to use
# GPU acceleration.
if [ "$GPU_ACCELERATED" == "true" ]; then
  export DISPLAY=":0.0"
  export GPU_MODE="host"
  Xvfb "$DISPLAY" -screen 0 1920x1080x16 -nolisten tcp &
else
  export GPU_MODE="swiftshader_indirect"
fi

# Asynchronously write updates on the standard output
# about the state of the boot sequence.
wait_for_boot &

# Start the emulator with no audio, no GUI, and no snapshots.
echo "Starting the emulator ..."
echo "OPTIONS:"
echo "SKIP ADB AUTH - $OPT_SKIP_AUTH"
echo "GPU           - $GPU_MODE"
echo "MEMORY        - $OPT_MEMORY"
echo "CORES         - $OPT_CORES"
emulator \
  -avd android \
  -gpu "$GPU_MODE" \
  -memory $OPT_MEMORY \
  -no-boot-anim \
  -cores $OPT_CORES \
  -ranchu \
  $AUTH_FLAG \
  -no-window \
  -no-snapshot  || update_state "ANDROID_STOPPED"


  # -qemu \
  # -smp 8,sockets=1,cores=4,threads=2,maxcpus=8