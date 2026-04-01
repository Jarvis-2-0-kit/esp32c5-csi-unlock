# Pre-built Firmware — Flash Instructions

## Requirements
- ESP32-C5 board (any variant)
- USB cable
- Python 3 + esptool (`pip install esptool`)

## Quick Flash (no ESP-IDF needed)

```bash
# Install esptool
pip install esptool

# Flash all three files
esptool.py --chip esp32c5 -b 460800 \
  --before default-reset --after hard-reset \
  write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m \
  0x2000 bootloader.bin \
  0x8000 partition-table.bin \
  0x10000 esp32c5_csi_unlock.bin
```

## Before Flashing

Edit `WIFI_SSID` and `WIFI_PASS` in `firmware/main/csi_unlock_demo.c` and rebuild, OR use the pre-built binary with a WiFi network named `YOUR_SSID` with password `YOUR_PASSWORD` (not very useful — rebuild recommended).

## What to Expect

Monitor serial output at 115200 baud:

```
ESP32-C5 CSI Unlock — HE20 245 Subcarriers
Connecting to WiFi...
Connected — IP: 192.168.1.xxx
Collecting HT20 baseline (3 seconds)...
HT20 baseline: 53 subcarriers, 42 frames

>>> Applying HE20 CSI unlock: REG 0x600A409C = 0x0A <<<

Register 0x600A409C: 0x0000001A -> 0x0000000A
Collecting HE20 data (3 seconds)...
HE20 result: 245 subcarriers, 38 frames

*** SUCCESS: HE20 CSI unlocked! 245 subcarriers ***
*** 4.7x improvement over default HT20 (53 sub)  ***
```

## Notes

- Router must support 802.11ax (WiFi 6) on 5 GHz for 245 subcarriers
- With non-WiFi 6 routers, you'll still get 53 subcarriers (HT20)
- The unlock is non-persistent — it resets on reboot (firmware re-applies it automatically)
