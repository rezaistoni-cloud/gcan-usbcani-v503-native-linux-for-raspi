# Reverse engineering notes

This project was built from real USB hook logs of `libECanVci.so`.

## USB device

```text
VID:PID = 0c66:000c
Product = USBCANI-V503
```

## Endpoints

```text
Bulk OUT = 0x02
Bulk IN  = 0x82
Int IN   = 0x81
```

The current implementation uses Bulk OUT `0x02` and Bulk IN `0x82`.

## CAN1 / 500K init sequence

Captured packets used in this driver:

```text
81 86 00 00 40 08 00 00 00 00 00 00 00 00
81 0C 00 00 40 08 00 00 00 00 21 00 00 00
81 01 00 00 40 08 03 00 00 00 00 00 00 00
81 0F 00 00 40 08 01 00 00 00 00 00 00 00
```

## TX format

Standard frame example:

```text
CAN ID = 0x123
DATA   = 11 22 33 44 55 66 77 88

USB:
21 23 01 00 00 08 11 22 33 44 55 66 77 88
```

Format:

```text
byte 0     = 0x21, TX CAN1
byte 1..4  = CAN ID little endian
byte 5     = DLC
byte 6..13 = data, padded to 8 bytes
```

Extended ID:

```text
encoded_id = can_id | 0x20000000
```

Example:

```text
ID 0x18DAF110 -> encoded 0x38DAF110 -> USB bytes 10 F1 DA 38
```

## RX format

RX arrives as 16-byte records:

```text
byte 0..3   = encoded CAN ID little endian
byte 4      = DLC
byte 5..12  = data bytes
byte 13..15 = timestamp/status-like values, not decoded yet
```

## Known limitations

- CAN1 only
- 500K only
- Classic CAN only
- CAN FD is not supported by this adapter
