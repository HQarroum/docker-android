FROM adoptopenjdk/openjdk11:alpine-jre

# Docker labels.
LABEL maintainer "Halim Qarroum <hqm.post@gmail.com>"
LABEL description "A Docker image allowing to run an Android emulator"
LABEL version "1.0.0"

# `redir.c` will be used to redirect
# localhost ADB ports to the container interface.
COPY deps/redir/redir.c /usr/src/redir.c

# Installing required packages.
RUN apk update && \
	apk upgrade && \
	apk add --no-cache \
		alpine-sdk \
		bash \
		unzip \
		wget \
		libvirt-daemon \
		dbus \
		polkit \
		mesa \
		mesa-dev \
		mesa-gl \
		virt-manager && \
	# Compile `redir`.
	gcc /usr/src/redir.c -o /usr/bin/redir && \
	strip /usr/bin/redir && \
	# Cleanup APK.
	apk del alpine-sdk && \
	rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /var/cache/apk/*

# Arguments that can be overriden at build-time.
ARG INSTALL_ANDROID_SDK=1
ARG API_LEVEL=33
ARG IMG_TYPE=google_apis
ARG ARCHITECTURE=x86_64
ARG CMD_LINE_VERSION=9477386_latest
ARG DEVICE_ID=pixel

# Environment variables.
ENV ANDROID_SDK_ROOT=/opt/android \
    ANDROID_PLATFORM_VERSION="platforms;android-$API_LEVEL" \
    PACKAGE_PATH="system-images;android-${API_LEVEL};${IMG_TYPE};${ARCHITECTURE}" \
    API_LEVEL=$API_LEVEL \
		DEVICE_ID=$DEVICE_ID \
    ARCHITECTURE=$ARCHITECTURE \
    ABI=${IMG_TYPE}/${ARCHITECTURE} \
    QTWEBENGINE_DISABLE_SANDBOX=1 \
    ANDROID_EMULATOR_WAIT_TIME_BEFORE_KILL=10

# Exporting environment variables for keeping in the path
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
COPY keys/* /root/.android/

# Copy the startup scripts.
COPY scripts/* /opt/

# Make the scripts executable.
RUN chmod +x /opt/*.sh

# This layer will download the Android command-line tools
# to install the Android SDK, emulator and system images.
# It will then install the Android SDK and emulator.
RUN /opt/install-sdk.sh

# Set the entrypoint
ENTRYPOINT ["/opt/start-emulator.sh"]