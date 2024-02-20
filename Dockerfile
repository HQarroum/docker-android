FROM openjdk:18-jdk-slim

ENV DEBIAN_FRONTEND noninteractive

#WORKDIR /
#=============================
# Install Dependenices 
#=============================
SHELL ["/bin/bash", "-c"]   

RUN apt update && apt install -y curl \
	sudo wget unzip bzip2 libdrm-dev \
	libxkbcommon-dev libgbm-dev libasound-dev libnss3 \
	libxcursor1 libpulse-dev libxshmfence-dev \
	xauth xvfb x11vnc fluxbox wmctrl libdbus-glib-1-2 socat \
	virt-manager


# Docker labels.
LABEL maintainer "Halim Qarroum <hqm.post@gmail.com>"
LABEL description "A Docker image allowing to run an Android emulator"
LABEL version "1.0.0"


# Arguments that can be overriden at build-time.
ARG INSTALL_ANDROID_SDK=1
ARG API_LEVEL=33
ARG IMG_TYPE=google_apis
ARG ARCHITECTURE=x86_64
ARG CMD_LINE_VERSION=9477386_latest
ARG DEVICE_ID=pixel
ARG GPU_ACCELERATED=false

# Environment variables.
ENV ANDROID_SDK_ROOT=/opt/android \
	ANDROID_PLATFORM_VERSION="platforms;android-$API_LEVEL" \
	PACKAGE_PATH="system-images;android-${API_LEVEL};${IMG_TYPE};${ARCHITECTURE}" \
	API_LEVEL=$API_LEVEL \
	DEVICE_ID=$DEVICE_ID \
	ARCHITECTURE=$ARCHITECTURE \
	ABI=${IMG_TYPE}/${ARCHITECTURE} \
	GPU_ACCELERATED=$GPU_ACCELERATED \
	QTWEBENGINE_DISABLE_SANDBOX=1 \
	ANDROID_EMULATOR_WAIT_TIME_BEFORE_KILL=10

# Exporting environment variables to keep in the path
# Android SDK binaries and shared libraries.
ENV PATH "${PATH}:${ANDROID_SDK_ROOT}/platform-tools"
ENV PATH "${PATH}:${ANDROID_SDK_ROOT}/emulator"
ENV PATH "${PATH}:${ANDROID_SDK_ROOT}/cmdline-tools/tools/bin"
ENV LD_LIBRARY_PATH "$ANDROID_SDK_ROOT/emulator/lib64:$ANDROID_SDK_ROOT/emulator/lib64/qt/lib"

# Set the working directory to /opt
WORKDIR /opt

# Exposing the Android emulator console port
# and the ADB port.
EXPOSE 5554 5555

# Initializing the required directories.
RUN mkdir /root/.android/ && \
	touch /root/.android/repositories.cfg

# Exporting ADB keys.
#COPY keys/* /root/.android/

# The following layers will download the Android command-line tools
# to install the Android SDK, emulator and system images.
# It will then install the Android SDK and emulator.
COPY scripts/install-sdk.sh /opt/
RUN chmod +x /opt/install-sdk.sh
RUN /opt/install-sdk.sh

# Copy the container scripts in the image.
COPY scripts/start-emulator.sh /opt/
COPY scripts/emulator-monitoring.sh /opt/
RUN chmod +x /opt/*.sh

# Set the entrypoint
ENTRYPOINT ["/opt/start-emulator.sh"]