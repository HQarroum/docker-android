#!/bin/bash

set -e

# Writes a log with a key and a value
# as a JSON object on the standard output.
function write_log() {
  echo "{ \"type\": \"$1\", \"value\": \"$2\" }"
}

# Writes a state update on the standard output.
function update_state() {
  write_log "state-update" "$1"
}

# Waits for the emulator to boot and writes
# state updates on the standard output.
function wait_for_boot() {
  update_state "ANDROID_BOOTING"

  # Waiting for the ADB server to start.
  while [ -n "$(adb wait-for-device > /dev/null)" ]; do
    adb wait-for-device
    sleep 1
  done
  
  # Waiting for the boot sequence to be completed.
  COMPLETED=$(adb shell getprop sys.boot_completed | tr -d '\r')
  while [ "$COMPLETED" != "1" ]; do
    COMPLETED=$(adb shell getprop sys.boot_completed | tr -d '\r')
    sleep 5
  done
  update_state "ANDROID_READY"
}