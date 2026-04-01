# ESP32-C5 CSI Unlock — 53 → 245 Subcarriers + HT40 PHY Patch

> **First documented reverse engineering of ESP32-C5 WiFi MAC registers.**
> Unlocks HE20 CSI (245 subcarriers) and forces HT40 PHY bandwidth via blob patching.

The ESP32-C5 is Espressif's first dual-band WiFi 6 (802.11ax) RISC-V microcontroller. The ESP-IDF CSI API only exposes HT20 data (53 subcarriers). This project unlocks HE20 CSI with **245 subcarriers — a 4.7× improvement** — and patches the proprietary WiFi blob to negotiate **HT40 (40 MHz) PHY bandwidth**.

## What You Get

| Feature | Default | After Patch |
|---------|---------|-------------|
| CSI subcarriers | 53 (HT20) | **245 (HE20)** |
| Subcarrier spacing | 312.5 kHz | **78.125 kHz** |
| PHY bandwidth | CBW20 (20 MHz) | **HT40 (40 MHz)** |
| TX power | ~13 dBm | **18 dBm** |

## Quick Start

### One-Line CSI Unlock

Add this after `esp_wifi_set_csi(true)` in any ESP32-C5 project:

```c
*(volatile uint32_t *)0x600A409C = 0x0A;  // 53 → 245 subcarriers
```

### Full Patch (CSI + HT40 PHY + Max Power)

Add linker wraps in your `CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=hal_mac_set_csi_cbw"
    "-Wl,--wrap=ieee80211_set_phy_bw"
)
```

Add wrap functions in your source:

```c
// Prevent blob from resetting CSI mode to HT20
extern void __real_hal_mac_set_csi_cbw(uint8_t cbw);
void __wrap_hal_mac_set_csi_cbw(uint8_t cbw) {
    *(volatile uint32_t *)0x600A409C = 0x0A;  // Force HE20
}

// Force 40 MHz PHY bandwidth (default is 20 MHz)
extern void __real_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi);
void __wrap_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi) {
    __real_ieee80211_set_phy_bw(iface, 2, sgi);  // 2 = BW40
}
```

After WiFi init, set max TX power:

```c
esp_wifi_set_max_tx_power(84);  // 21 dBm max (hardware may cap at 18 dBm)
```

## Hardware Limits (Confirmed)

| Parameter | Maximum | Notes |
|-----------|---------|-------|
| CSI subcarriers | **245** | HE20 mode, 20 MHz CSI frontend — silicon limit |
| PHY bandwidth | **HT40** | BW80/BW160 falls back to HT40 |
| TX power | **18 dBm** | 72 units (requested 84, hardware caps at 72) |
| MIMO streams | **1×1** | Single antenna, single stream |
| Max PHY rate | **172 Mbps** | HE20 MCS11 1SS |

HE40 (484 sub), HE80 (996 sub), and HE160 (1992 sub) are **not achievable** — the CSI capture engine has a fixed 20 MHz RF frontend hardwired in silicon. See [detailed analysis](#why-he40-fails) below.

## Hardware Tested

- **Board**: DFRobot FireBeetle ESP32-C5 V1.0
- **Chip**: ESP32-C5 (revision v1.0), single-core RISC-V @ 240 MHz
- **ESP-IDF**: v5.5 (master branch, `--preview` target)
- **Router**: Sagemcom (Sunrise ISP), 5 GHz, 802.11a/n/ac/ax mixed mode

## Reverse Engineering Process

### 1. Blob Disassembly

Extracted function names and register addresses from `libpp.a` and `libphy.a`:

```bash
riscv32-esp-elf-nm libpp.a | grep hal_mac     # → found hal_mac_set_csi, hal_mac_set_csi_cbw, etc.
riscv32-esp-elf-objdump -d libpp.a             # → extracted register addresses from LUI instructions
```

Key discovery — `hal_mac_set_csi` writes to `0x600A4098`, revealing WiFi MAC base at `0x600A4000`.

### 2. Live Register Dump

Dumped all non-zero registers in `0x600A0000–0x600AFFFF` from a running ESP32-C5. Found **1,241 active registers**.

### 3. CSI Mode Register (0x600A409C)

Systematically tested all values `0x00–0x3E` measuring CSI subcarrier count:

| Value | Binary | Subcarriers | Meaning |
|-------|--------|-------------|---------|
| `0x1A` | `011010` | 53 | Default — bit 4 forces HT20 |
| **`0x0A`** | **`001010`** | **245** | **Bit 4 cleared → HE20 unlocked** |
| `0x0C` | `001100` | 245 | Also works |
| `0x0E` | `001110` | 245 | Also works |

**Bit 4 = "force legacy" flag.** Clearing it enables WiFi 6 HE CSI capture.

### 4. PHY Bandwidth Patch

The blob calls `ieee80211_set_phy_bw(iface, 1, sgi)` during connection — `bw=1` means CBW20. Using `--wrap` linker flag, we intercept this call and force `bw=2` (HT40):

```
Before patch: wifi: phytype:CBW20-SGI, snr:54, maxRate:172
After patch:  wifi: phytype:HT40-SGI, snr:52, maxRate:172
```

### 5. Blob Reset Prevention

The blob calls `hal_mac_set_csi_cbw(0)` during WiFi operation, which resets the CSI mode register. Our `--wrap` intercept forces HE20 mode on every call, preventing the reset.

### 6. Empty Stub Discovery

`hal_mac_set_csi_cbw` in the blob is an empty function — just `ret`:

```asm
00000000 <hal_mac_set_csi_cbw>:
   0:   8082    ret
```

Espressif defined the CSI bandwidth control interface but never implemented it. This may be planned for a future ESP-IDF release.

## WiFi MAC Register Map

| Address | Name | Description |
|---------|------|-------------|
| `0x600A4000` | `WIFI_MAC_BASE` | WiFi MAC register block |
| `0x600A4080` | `RX_CTRL` | RX DMA control (bit 0 = reload) |
| `0x600A4084` | `RX_DMA_BASE` | RX DMA descriptor base pointer |
| `0x600A4098` | `CSI_ENABLE` | CSI master enable (bit 23) |
| **`0x600A409C`** | **`CSI_MODE`** | **CSI acquisition mode — THE UNLOCK REGISTER** |
| `0x600A40A0` | `CHAN_CONFIG` | Channel/frequency configuration |
| `0x600A411C` | `CSI_FILTER` | Frame type filter (HT/VHT/HE bits) |
| `0x600A4400` | `BB_CHAN_BW` | Baseband channel bandwidth config |
| `0x600A4CA8` | `MAC_CTRL` | MAC control (init/deinit) |
| `0x600A9C18` | `DIGITAL_BW40` | Digital BW40 config (bit 2) |
| `0x600A0874` | `BW_SELECTOR` | Bandwidth selector (bits 17, 20) |
| `0x600AD800` | `MODEM_PWR` | Modem power status |

Full register dump (1,241 registers): [`dumps/full_register_dump.txt`](dumps/full_register_dump.txt)

Detailed register documentation: [`docs/register_map.md`](docs/register_map.md)

## Why HE40+ Fails

We conducted **5 rounds** of systematic experiments (150+ configurations) across MAC, PHY, and baseband registers:

| Round | Target | Method | Result |
|-------|--------|--------|--------|
| 1 | CSI mode register | `0x600A409C` full sweep | Max 245 (HE20) |
| 2 | Channel config | `0x600A40A0` + `0x600A40A4` combos | Max 245 |
| 3 | PHY baseband | `0x600A4400`, `0x600A9C18`, `0x600A0874` | Max 245 |
| 4 | CBW implementation | Own `hal_mac_set_csi_cbw()` + combos | Max 245 |
| 5 | PHY BW80 patch | `--wrap ieee80211_set_phy_bw` bw=3 | PHY stays HT40, CSI stays 245 |

The 245 limit is enforced at three levels:
1. **CSI capture frontend** — fixed 20 MHz bandwidth in silicon
2. **PHY** — max HT40 (BW80 attempt falls back to HT40)
3. **Blob** — `hal_mac_set_csi_cbw` is unimplemented

Full experiment logs: [`dumps/`](dumps/)

## Project Structure

```
esp32c5-csi-unlock/
├── README.md
├── LICENSE
├── firmware/
│   ├── main/
│   │   ├── csi_unlock_demo.c           # Clean demo firmware
│   │   ├── raw_mac.h                   # Register definitions
│   │   ├── wraith_node_cbw40_patch.c   # HT40 PHY patch firmware
│   │   ├── wraith_node_bw80_patch.c    # BW80 attempt firmware
│   │   └── CMakeLists.txt
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults
├── releases/
│   ├── esp32c5_csi_unlock.bin          # Pre-built binary
│   ├── bootloader.bin
│   ├── partition-table.bin
│   └── FLASH_INSTRUCTIONS.md
├── tools/
│   ├── reg_dump.c                      # Register dump utility
│   └── bw_experiment.c                 # BW experiment firmware
├── dumps/
│   ├── full_register_dump.txt          # 1,241 registers from live chip
│   ├── bw_experiment_1.txt             # CSI mode sweep (CBW20)
│   ├── bw_experiment_2.txt             # A0+9C combos
│   ├── bw_experiment_3_phy_bb.txt      # PHY baseband experiments
│   ├── bw_experiment_4_cbw_phy.txt     # CBW implementation tests
│   └── bw_experiment5_ht40.txt         # CSI mode sweep (HT40 PHY)
└── docs/
    └── register_map.md                 # Detailed register documentation
```

## Building

```bash
export IDF_PATH=~/esp-idf
source $IDF_PATH/export.sh
cd firmware
idf.py --preview set-target esp32c5
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Pre-built Binary

Flash without building (requires `esptool`):

```bash
pip install esptool
cd releases
esptool.py --chip esp32c5 -b 460800 \
  write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m \
  0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 esp32c5_csi_unlock.bin
```

Note: Pre-built binary has placeholder WiFi credentials. Rebuild from source with your SSID/password.

## Applications

- **WiFi CSI sensing** — 4.7× frequency resolution improvement
- **Channel Impulse Response** — finer delay estimation
- **WiFi radar / SAR** — improved range profile
- **Indoor positioning** — more multipath components resolved
- **Gesture recognition** — richer feature vectors
- **Breathing / vital sign detection** — higher SNR
- **Security research** — WiFi PHY analysis with raw register access

## Related Work

- [esp32-open-mac](https://github.com/esp32-open-mac/esp32-open-mac) — Open-source WiFi MAC for classic ESP32
- [ESP32-C3 WiFi Driver RE](https://arxiv.org/html/2501.17684v3) — Academic RE of ESP32-C3 WiFi drivers
- [Holl & Reinhard (2017)](https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.118.183901) — WiFi holography using CSI

## License

MIT — see [LICENSE](LICENSE).

## Disclaimer

This project writes to undocumented hardware registers and patches proprietary blob functions via linker wraps. While no damage or instability has been observed, **use at your own risk**. Register addresses may differ across ESP32-C5 revisions.
