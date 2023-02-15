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
redir --laddr=$ip --lport=$EMULATOR_CONSOLE_PORT --caddr=127.0.0.1 --cport=$EMULATOR_CONSOLE_PORT &
redir --laddr=$ip --lport=$ADB_PORT --caddr=127.0.0.1 --cport=$ADB_PORT &

export USER=root

# Creating the Android Virtual Emulator.
echo "Creating the Android Virtual Emulator ..."
echo no | avdmanager create avd -n android --abi ${ABI} -k "$PACKAGE_PATH" --device "pixel"

# Asynchronously write updates on the standard output
# about the state of the boot sequence.
#cat ~/.android/avd/android.avd/config.ini
wait_for_boot &

# Start the emulator with no audio, no GUI, and no snapshots.
echo "Starting the emulator ..."
if emulator \
  -avd android \
  -noaudio \
  -no-boot-anim \
  -no-window \
  -no-snapshot-save \
  -qemu \
  -enable-kvm;
then
  update_state "stopped"
else 
  update_state "stopped"
fi
