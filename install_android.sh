#!/bin/bash

# --- CONFIGURATION ---
ANDROID_VERSION="api-33" # Android 13
REPO_URL="https://github.com/HQarroum/docker-android"
DIR_NAME="docker-android"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== STARTING ANDROID DOCKER INSTALLATION ===${NC}"

# 1. Check Root Privileges
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}[ERROR] Please run this script as root (sudo ./install_android.sh)${NC}"
  exit 1
fi

# 2. Check KVM Support (Hardware Virtualization)
echo -e "${GREEN}[1/5] Checking KVM support (Hardware Virtualization)...${NC}"
if [ -r /dev/kvm ]; then
    echo "OK: KVM device (/dev/kvm) is ready."
else
    echo -e "${RED}[WARNING] /dev/kvm not found. The emulator will be very slow or fail to start.${NC}"
    echo "If running on VMWare/ESXi, ensure 'Virtualize Intel VT-x/EPT' (Nested Virtualization) is enabled."
    # Ask user to continue or abort
    read -p "Do you want to continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then exit 1; fi
fi

# 3. Check and Install Docker & Dependencies
echo -e "${GREEN}[2/5] Checking and installing Docker...${NC}"
apt-get update -qq
apt-get install -y git curl apt-transport-https ca-certificates software-properties-common -qq

if ! command -v docker &> /dev/null; then
    echo "Docker is not installed. Installing automatically..."
    curl -fsSL https://get.docker.com -o get-docker.sh
    sh get-docker.sh
    rm get-docker.sh
    echo "Docker installed successfully."
else
    echo "OK: Docker is already installed."
fi

# 4. Clone Source Code
echo -e "${GREEN}[3/5] Cloning docker-android source code...${NC}"
if [ -d "$DIR_NAME" ]; then
    echo "Directory $DIR_NAME exists. Removing to reinstall..."
    rm -rf "$DIR_NAME"
fi

git clone $REPO_URL
cd $DIR_NAME

# 5. Build and Run Container
echo -e "${GREEN}[4/5] Building and Starting Container (This may take 5-10 minutes)...${NC}"
# Check for docker compose v2 or legacy docker-compose v1
if docker compose version &> /dev/null; then
    docker compose up -d --build android-emulator
elif docker-compose version &> /dev/null; then
    docker-compose up -d --build android-emulator
else
    echo -e "${RED}[ERROR] 'docker compose' command not found. Please check your Docker installation.${NC}"
    exit 1
fi

# 6. Final Check and Output Info
echo -e "${GREEN}[5/5] Checking status...${NC}"
sleep 5 # Wait a moment for container initialization

if docker ps | grep -q "android-emulator"; then
    SERVER_IP=$(hostname -I | awk '{print $1}')
    echo -e "\n${GREEN}=== INSTALLATION COMPLETED SUCCESSFULLY ===${NC}"
    echo -e "The container is running. Connection details:"
    echo "---------------------------------------------------"
    echo -e "Server IP (Ubuntu):  ${GREEN}$SERVER_IP${NC}"
    echo -e "ADB Port:            ${GREEN}5555${NC}"
    echo "---------------------------------------------------"
    echo -e ">>> Connection command for WINDOWS (CMD/Powershell):"
    echo -e "${GREEN}adb connect $SERVER_IP:5555${NC}"
    echo -e ">>> Scrcpy command (Screen Mirroring):"
    echo -e "${GREEN}scrcpy -s $SERVER_IP:5555${NC}"
    echo "---------------------------------------------------"
else
    echo -e "${RED}[ERROR] Container is not running. Check logs using: docker logs android-emulator${NC}"
fi
