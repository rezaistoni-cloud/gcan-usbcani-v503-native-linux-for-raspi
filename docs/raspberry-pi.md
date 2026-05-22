# Raspberry Pi notes

## Dependencies

```bash
sudo apt update
sudo apt install -y build-essential libusb-1.0-0-dev pkg-config can-utils usbutils
```

## Check USB devices

```bash
lsusb
```

Expected GCAN:

```text
0c66:000c Rexon Electronics Corp. USBCANI-V503
```

If using PEAK PCAN at the same time, it may appear as SocketCAN `can0/can1`.

## Build

```bash
make clean
make
```

## Run

```bash
sudo ./build/gcan-native-tool rx
sudo ./build/gcan-native-tool tx --id 123 --data 1122334455667788
```
