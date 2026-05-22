#!/usr/bin/env bash
set -euo pipefail
sudo apt update
sudo apt install -y build-essential libusb-1.0-0-dev pkg-config can-utils usbutils
echo "Done."
