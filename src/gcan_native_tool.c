#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <libusb-1.0/libusb.h>

#define GCAN_VID 0x0c66
#define GCAN_PID 0x000c

#define GCAN_IFACE 0
#define EP_BULK_OUT 0x02
#define EP_BULK_IN  0x82

static libusb_device_handle *devh = NULL;
static volatile int running = 1;
static int verbose = 0;

static void on_sigint(int sig) { (void)sig; running = 0; }

static void hexdump(const char *prefix, const uint8_t *buf, int len)
{
    printf("%s", prefix);
    for (int i = 0; i < len; i++) {
        printf("%02X", buf[i]);
        if (i + 1 < len) printf(" ");
    }
    printf("\n");
}

static uint32_t le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int usb_write_packet(const uint8_t *pkt, int len)
{
    int transferred = 0;
    if (verbose) hexdump("USB OUT: ", pkt, len);

    int rc = libusb_bulk_transfer(devh, EP_BULK_OUT, (unsigned char *)pkt, len, &transferred, 1000);
    if (rc != 0) {
        fprintf(stderr, "ERROR: bulk OUT failed rc=%d (%s)\n", rc, libusb_error_name(rc));
        return 0;
    }
    if (transferred != len) {
        fprintf(stderr, "ERROR: bulk OUT short write %d/%d\n", transferred, len);
        return 0;
    }
    return 1;
}

static int usb_read_once(uint8_t *buf, int buflen, int timeout_ms, int show)
{
    int transferred = 0;
    int rc = libusb_bulk_transfer(devh, EP_BULK_IN, buf, buflen, &transferred, timeout_ms);
    if (rc == LIBUSB_ERROR_TIMEOUT) return 0;
    if (rc != 0) return 0;
    if (transferred > 0 && (show || verbose)) hexdump("USB IN : ", buf, transferred);
    return transferred;
}

static void drain_usb(int loops, int timeout_ms)
{
    uint8_t buf[64];
    for (int i = 0; i < loops; i++) usb_read_once(buf, sizeof(buf), timeout_ms, 0);
}

static int gcan_open(void)
{
    int rc = libusb_init(NULL);
    if (rc != 0) {
        fprintf(stderr, "ERROR: libusb_init failed rc=%d\n", rc);
        return 0;
    }

    devh = libusb_open_device_with_vid_pid(NULL, GCAN_VID, GCAN_PID);
    if (!devh) {
        fprintf(stderr, "ERROR: cannot open GCAN device %04X:%04X\n", GCAN_VID, GCAN_PID);
        return 0;
    }

    printf("GCAN USB opened: %04X:%04X\n", GCAN_VID, GCAN_PID);

    libusb_set_auto_detach_kernel_driver(devh, 1);

    if (libusb_kernel_driver_active(devh, GCAN_IFACE) == 1) {
        printf("Kernel driver active, detaching...\n");
        libusb_detach_kernel_driver(devh, GCAN_IFACE);
    }

    rc = libusb_claim_interface(devh, GCAN_IFACE);
    if (rc != 0) {
        fprintf(stderr, "ERROR: claim interface failed rc=%d (%s)\n", rc, libusb_error_name(rc));
        libusb_close(devh);
        devh = NULL;
        return 0;
    }

    printf("GCAN interface claimed\n");
    return 1;
}

static void gcan_close(void)
{
    if (!devh) return;

    uint8_t close_cmd[14] = {
        0x81, 0xA0, 0x00, 0x00, 0x40, 0x08, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00
    };

    usb_write_packet(close_cmd, sizeof(close_cmd));
    usleep(100 * 1000);

    libusb_release_interface(devh, GCAN_IFACE);
    libusb_close(devh);
    devh = NULL;

    libusb_exit(NULL);
    printf("GCAN USB closed\n");
}

static int gcan_init_can1_500k(void)
{
    uint8_t poll_cmd[14]  = {0x81,0x86,0x00,0x00,0x40,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t init_cmd[14]  = {0x81,0x0C,0x00,0x00,0x40,0x08,0x00,0x00,0x00,0x00,0x21,0x00,0x00,0x00};
    uint8_t cfg_cmd[14]   = {0x81,0x01,0x00,0x00,0x40,0x08,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8_t start_cmd[14] = {0x81,0x0F,0x00,0x00,0x40,0x08,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    printf("GCAN init CAN1 500K native sequence...\n");

    for (int i = 0; i < 4; i++) {
        if (!usb_write_packet(poll_cmd, sizeof(poll_cmd))) return 0;
        usleep(30 * 1000);
        drain_usb(2, 20);
    }

    if (!usb_write_packet(init_cmd, sizeof(init_cmd))) return 0;
    usleep(50 * 1000);
    drain_usb(3, 20);

    if (!usb_write_packet(cfg_cmd, sizeof(cfg_cmd))) return 0;
    usleep(50 * 1000);
    drain_usb(3, 20);

    if (!usb_write_packet(start_cmd, sizeof(start_cmd))) return 0;
    usleep(150 * 1000);
    drain_usb(10, 20);

    printf("GCAN init/start sequence done\n");
    return 1;
}

static int parse_data_hex(const char *input, uint8_t *data)
{
    char clean[64];
    int n = 0;

    for (int i = 0; input[i] != '\0' && n < (int)sizeof(clean) - 1; i++) {
        if (isxdigit((unsigned char)input[i])) clean[n++] = input[i];
    }

    clean[n] = '\0';

    if (n == 0 || (n % 2) != 0) {
        fprintf(stderr, "ERROR: data hex length must be even\n");
        return -1;
    }

    int len = n / 2;
    if (len > 8) {
        fprintf(stderr, "ERROR: Classic CAN max data is 8 bytes\n");
        return -1;
    }

    for (int i = 0; i < len; i++) {
        char b[3];
        b[0] = clean[i * 2];
        b[1] = clean[i * 2 + 1];
        b[2] = 0;
        data[i] = (uint8_t)strtoul(b, NULL, 16);
    }

    return len;
}

static uint32_t parse_can_id(const char *s)
{
    if (!s) return 0;
    if (strlen(s) > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return (uint32_t)strtoul(s, NULL, 16);
    return (uint32_t)strtoul(s, NULL, 16);
}

static int gcan_tx_can1(uint32_t can_id, int ext, const uint8_t *data, int dlc)
{
    uint32_t encoded_id = can_id;
    if (ext) encoded_id |= 0x20000000U;

    uint8_t pkt[14];
    memset(pkt, 0, sizeof(pkt));

    pkt[0] = 0x21;
    pkt[1] = (uint8_t)(encoded_id & 0xFF);
    pkt[2] = (uint8_t)((encoded_id >> 8) & 0xFF);
    pkt[3] = (uint8_t)((encoded_id >> 16) & 0xFF);
    pkt[4] = (uint8_t)((encoded_id >> 24) & 0xFF);
    pkt[5] = (uint8_t)dlc;

    for (int i = 0; i < dlc && i < 8; i++) pkt[6 + i] = data[i];

    printf("Native TX CAN1 id=0x%X ext=%d dlc=%d\n", can_id, ext, dlc);

    if (!usb_write_packet(pkt, sizeof(pkt))) return 0;

    usleep(1000 * 1000);
    drain_usb(10, 20);

    return 1;
}

static int parse_can_frame_record(const uint8_t *rec)
{
    uint32_t raw_id = le32(rec);
    uint8_t dlc = rec[4];

    if (dlc > 8) return 0;

    int ext = (raw_id & 0x20000000U) ? 1 : 0;
    uint32_t can_id;

    if (ext) {
        can_id = raw_id & 0x1FFFFFFFU;
    } else {
        can_id = raw_id;
        if (can_id > 0x7FFU) return 0;
    }

    if (ext) printf("RX id=0x%08X ext=1 rtr=0 dlc=%u data=", can_id, dlc);
    else printf("RX id=0x%03X ext=0 rtr=0 dlc=%u data=", can_id, dlc);

    for (int i = 0; i < dlc; i++) {
        printf("%02X", rec[5 + i]);
        if (i + 1 < dlc) printf(" ");
    }

    printf("\n");
    fflush(stdout);

    return 1;
}

static void gcan_rx_loop(void)
{
    uint8_t buf[64];

    printf("Native RX CAN1 started. Press Ctrl+C to stop.\n");

    drain_usb(20, 20);

    while (running) {
        int n = usb_read_once(buf, sizeof(buf), 100, 0);
        if (n <= 0) continue;
        if ((n % 16) != 0) continue;

        for (int off = 0; off + 16 <= n; off += 16) parse_can_frame_record(&buf[off]);
    }

    printf("\nNative RX stopped\n");
}

static void usage(const char *prog)
{
    printf("\n");
    printf("GCAN Native Tool - libusb, no libECanVci.so\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s tx --id <hex> --data <hex> [--ext] [--count N] [--verbose]\n", prog);
    printf("  %s rx [--verbose]\n", prog);
    printf("\n");
    printf("Current limitation: CAN1 only, 500K only, Classic CAN only\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    int mode_tx = strcmp(argv[1], "tx") == 0;
    int mode_rx = strcmp(argv[1], "rx") == 0;

    if (!mode_tx && !mode_rx) {
        usage(argv[0]);
        return 1;
    }

    uint32_t can_id = 0;
    int has_id = 0;
    int ext = 0;
    int count = 1;
    const char *data_hex = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            can_id = parse_can_id(argv[++i]);
            has_id = 1;
        } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data_hex = argv[++i];
        } else if (strcmp(argv[i], "--ext") == 0) {
            ext = 1;
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count < 1) count = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "ERROR: unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    uint8_t data[8];
    memset(data, 0, sizeof(data));
    int dlc = 0;

    if (mode_tx) {
        if (!has_id || !data_hex) {
            fprintf(stderr, "ERROR: tx requires --id and --data\n");
            usage(argv[0]);
            return 1;
        }

        if (!ext && can_id > 0x7FF) {
            fprintf(stderr, "ERROR: standard CAN ID max is 0x7FF. Use --ext for 29-bit ID.\n");
            return 1;
        }

        if (ext && can_id > 0x1FFFFFFF) {
            fprintf(stderr, "ERROR: extended CAN ID max is 0x1FFFFFFF.\n");
            return 1;
        }

        dlc = parse_data_hex(data_hex, data);
        if (dlc < 0) return 1;
    }

    if (!gcan_open()) return 1;

    if (!gcan_init_can1_500k()) {
        gcan_close();
        return 1;
    }

    int ok = 1;

    if (mode_tx) {
        for (int i = 0; i < count; i++) {
            ok = gcan_tx_can1(can_id, ext, data, dlc);
            if (!ok) break;
            printf("TX %d/%d done\n", i + 1, count);
        }
    } else if (mode_rx) {
        gcan_rx_loop();
    }

    gcan_close();

    if (!ok) {
        fprintf(stderr, "ERROR: operation failed\n");
        return 1;
    }

    return 0;
}
