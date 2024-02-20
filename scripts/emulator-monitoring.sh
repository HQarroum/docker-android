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

function disable_animation() {
  adb shell "settings put global window_animation_scale 0.0"
  adb shell "settings put global transition_animation_scale 0.0"
  adb shell "settings put global animator_duration_scale 0.0"
  echo "...Disable animations"
};

function hidden_policy() {
  adb shell "settings put global hidden_api_policy_pre_p_apps 1;settings put global hidden_api_policy_p_apps 1;settings put global hidden_api_policy 1"
  echo "...Hidden policy"
};


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
  sleep 1
  if [ "$DISABLE_ANIMATION" = "true" ]; then
  disable_animation
  sleep 1
  fi

  if [ "$DISABLE_HIDDEN_POLICY" = "true" ]; then
  hidden_policy
  sleep 1
  fi
  update_state "ANDROID_READY"
}