#!/bin/bash

set -e

# If the installation flag of the Android SDK is set
# we download the Android command-line tools,
# install the SDK, platform tools and the emulator.
if [ "$INSTALL_ANDROID_SDK" == "1" ]; then
  echo "Installing the Android SDK, platform tools and emulator ..."
  wget https://dl.google.com/android/repository/commandlinetools-linux-${CMD_LINE_VERSION}.zip -P /tmp && \
  mkdir -p $ANDROID_SDK_ROOT/cmdline-tools/ && \
  unzip -d $ANDROID_SDK_ROOT/cmdline-tools/ /tmp/commandlinetools-linux-${CMD_LINE_VERSION}.zip && \
  mv $ANDROID_SDK_ROOT/cmdline-tools/cmdline-tools/ $ANDROID_SDK_ROOT/cmdline-tools/tools/ && \
  rm /tmp/commandlinetools-linux-${CMD_LINE_VERSION}.zip && \
  yes | sdkmanager --licenses && \
  sdkmanager --install "$PACKAGE_PATH" "$ANDROID_PLATFORM_VERSION" platform-tools emulator
fi
