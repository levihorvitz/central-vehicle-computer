#!/usr/bin/env bash
# Creates and brings up vcan0, the virtual CAN interface used in development
# and by the SocketCAN tests. Safe to run more than once.
set -euo pipefail

sudo modprobe vcan
if ! ip link show vcan0 > /dev/null 2>&1; then
    sudo ip link add dev vcan0 type vcan
fi
sudo ip link set up vcan0
ip link show vcan0
