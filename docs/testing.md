# Testing

## PEAK / SocketCAN monitor

```bash
sudo ip link set can1 down 2>/dev/null || true
sudo ip link set can1 type can bitrate 500000 restart-ms 100
sudo ip link set can1 up
candump -L can1
```

## GCAN TX

```bash
sudo ./build/gcan-native-tool tx --id 123 --data 1122334455667788
sudo ./build/gcan-native-tool tx --id 18DAF110 --data 11223344 --ext
```

## GCAN RX

```bash
sudo ./build/gcan-native-tool rx
```

Then from PEAK/SocketCAN:

```bash
cansend can1 123#1122334455667788
cansend can1 321#AABBCCDD
cansend can1 18DAF110#11223344
```
