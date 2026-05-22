# GCAN USBCANI-V503 Native Linux Driver

Native userspace Linux driver/tool for **GCAN / ECAN / Rexon USBCANI-V503** USB-CAN adapter.

This project was created from real reverse-engineering and hardware testing on Raspberry Pi.

## Hardware tested

| Item | Status |
|---|---|
| Device | GCAN / ECAN / Rexon USBCANI-V503 |
| USB VID:PID | `0c66:000c` |
| Host | Raspberry Pi / Debian Linux |
| Reference CAN adapter | PEAK PCAN-USB Pro FD / SocketCAN |
| CAN mode | Classic CAN |
| Bitrate | 500 kbit/s |
| Standard CAN 11-bit | OK |
| Extended CAN 29-bit | OK |
| CAN FD | Not supported by this device |

> This is an experimental userspace `libusb-1.0` implementation. It does **not** make the GCAN appear as `can0/can1` SocketCAN device.

## Why

Vendor Linux SDK may depend on `libECanVci.so`, and available builds may not work on Raspberry Pi ARM64.  
This project talks directly to the USB device using `libusb-1.0`, so it can be compiled natively on Raspberry Pi.

## Current limitations

- CAN1 only
- 500 kbit/s only
- Classic CAN only
- TX/RX Standard and Extended frames
- No CAN2 yet
- No CAN FD
- No SocketCAN kernel driver yet

## Build

```bash
sudo apt update
sudo apt install -y build-essential libusb-1.0-0-dev pkg-config can-utils usbutils
make
```

Binary:

```bash
build/gcan-native-tool
```

## Quick TX test

Use another CAN adapter, for example PEAK PCAN with SocketCAN:

```bash
sudo ip link set can1 down 2>/dev/null || true
sudo ip link set can1 type can bitrate 500000 restart-ms 100
sudo ip link set can1 up
candump -L can1
```

Send from GCAN:

```bash
sudo ./build/gcan-native-tool tx --id 123 --data 1122334455667788
sudo ./build/gcan-native-tool tx --id 18DAF110 --data 11223344 --ext
```

Expected in `candump`:

```text
can1 123#1122334455667788
can1 18DAF110#11223344
```

## Quick RX test

```bash
sudo ./build/gcan-native-tool rx
```

From PEAK/SocketCAN side:

```bash
cansend can1 123#1122334455667788
cansend can1 321#AABBCCDD
cansend can1 18DAF110#11223344
```

Expected:

```text
RX id=0x123 ext=0 rtr=0 dlc=8 data=11 22 33 44 55 66 77 88
RX id=0x321 ext=0 rtr=0 dlc=4 data=AA BB CC DD
RX id=0x18DAF110 ext=1 rtr=0 dlc=4 data=11 22 33 44
```

## Install udev rule

Optional, to allow non-root access:

```bash
sudo cp udev/99-gcan-usbcani-v503.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Unplug and replug the USB device.

## Reverse-engineering notes

The core discovered protocol:

- Bulk OUT endpoint: `0x02`
- Bulk IN endpoint: `0x82`
- Standard TX frame example:
  `21 23 01 00 00 08 11 22 33 44 55 66 77 88`
- Extended TX frame encodes:
  `encoded_id = can_id | 0x20000000`

See `docs/reverse-engineering-notes.md`.

## License

MIT
