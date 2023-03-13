#!/bin/bash

set -e

source ./emulator-monitoring.sh

# The emulator console port. 
EMULATOR_CONSOLE_PORT=5554
# The ADB port used to connect to ADB.
ADB_PORT=5555

# Start ADB server by listening on all interfaces.
echo "Starting the ADB server ..."
adb -a -P 5037 server nodaemon &

# Detect ip and forward ADB ports outside to outside interface
ip=$(ip addr list eth0 | grep "inet " | cut -d' ' -f6 | cut -d/ -f1)
redir --laddr="$ip" --lport="$EMULATOR_CONSOLE_PORT" --caddr=127.0.0.1 --cport="$EMULATOR_CONSOLE_PORT" &
redir --laddr="$ip" --lport="$ADB_PORT" --caddr=127.0.0.1 --cport="$ADB_PORT" &

export USER=root

# Creating the Android Virtual Emulator.
echo "Creating the Android Virtual Emulator ..."
echo "Using package '$PACKAGE_PATH', ABI '$ABI' and device '$DEVICE_ID' for creating the emulator"
echo no | avdmanager create avd -f -n android --abi "$ABI" -k "$PACKAGE_PATH" --device "$DEVICE_ID"

# If GPU acceleration is enabled, we create a virtual framebuffer
# to be used by the emulator when running with GPU acceleration.
if [ "$GPU_ACCELERATED" == "true" ]; then
  echo "Running with GPU acceleration enabled"
  export DISPLAY=":0.0"
  export GPU_MODE="host"
  Xvfb "$DISPLAY" -screen 0 1920x1080x16 -nolisten tcp &
else
  export GPU_MODE="auto"
fi

# Asynchronously write updates on the standard output
# about the state of the boot sequence.
wait_for_boot &

# Start the emulator with no audio, no GUI, and no snapshots.
echo "Starting the emulator ..."
emulator \
  -verbose \
  -avd android \
  -gpu "$GPU_MODE" \
  -no-boot-anim \
  -no-window \
  -no-snapshot-save || update_state "ANDROID_STOPPED"
