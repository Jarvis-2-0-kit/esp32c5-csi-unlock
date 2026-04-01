# ESP32-C5 CSI Unlock — HE20 245 Subcarriers via Raw Register Access

**First documented reverse engineering of ESP32-C5 WiFi MAC registers to unlock HE20 CSI mode (245 subcarriers vs default 53).**

The ESP32-C5 is Espressif's first dual-band WiFi 6 (802.11ax) RISC-V microcontroller. While the ESP-IDF CSI API exposes only HT20 CSI data (53 subcarriers), the hardware is capable of capturing HE20 (High Efficiency) CSI with **245 subcarriers** — a **4.7× improvement** in frequency resolution.

This project documents the reverse engineering process, provides the register map, and includes ready-to-flash firmware that unlocks HE20 CSI on any ESP32-C5 board.

## Results

| Mode | Subcarriers | Spacing | Effective BW | Method |
|------|-------------|---------|-------------|--------|
| HT20 (default) | 53 | 312.5 kHz | 16.6 MHz | ESP-IDF API |
| **HE20 (unlocked)** | **245** | **78.125 kHz** | **19.1 MHz** | **Register 0x600A409C = 0x0A** |
| HE40 (attempted) | N/A | — | — | Not achievable — see [Why HE40+ Fails](#why-he40-fails) |
| HE80 (attempted) | N/A | — | — | Not achievable — see [Why HE40+ Fails](#why-he40-fails) |
| HE160 (attempted) | N/A | — | — | Not achievable — see [Why HE40+ Fails](#why-he40-fails) |

## The Unlock

After WiFi initialization and CSI enable via the standard ESP-IDF API, a single register write switches CSI capture from HT20 to HE20:

```c
#define REG32(addr) (*(volatile uint32_t *)(addr))

// After esp_wifi_set_csi(true):
REG32(0x600A409C) = 0x0A;  // Enable HE20 CSI (245 subcarriers)
```

That's it. One line. The CSI callback immediately starts receiving 245-subcarrier frames instead of 53.

### Why This Works

Register `0x600A409C` controls the CSI acquisition mode in the WiFi MAC hardware:

- **Default value `0x1A`** (binary `11010`): Bit 4 forces HT20 mode → 53 subcarriers
- **Value `0x0A`** (binary `01010`): Bit 4 cleared, bit 3 + bit 1 set → HE20 mode → 245 subcarriers

Bit 4 acts as a "force legacy" flag. Clearing it allows the hardware to capture WiFi 6 (HE) CSI frames natively.

### Other Working Values

| Value | Binary | n_sub | Notes |
|-------|--------|-------|-------|
| `0x0A` | `01010` | 245 | HE20 — recommended |
| `0x0C` | `01100` | 245 | HE20 — also works |
| `0x0E` | `01110` | 245 | HE20 — also works |
| `0x3C` | `111100` | 245 | HE20 with upper bits |
| `0x3E` | `111110` | 245 | HE20 with upper bits |
| `0x1A` | `11010` | 53 | Default HT20 |

## Hardware Tested

- **Board**: DFRobot FireBeetle ESP32-C5 V1.0
- **Chip**: ESP32-C5 (revision v1.0), single-core RISC-V @ 240 MHz
- **ESP-IDF**: v5.5 (master branch, `--preview` target)
- **Router**: Sagemcom (Sunrise ISP), 5 GHz channel 36, 802.11a/n/ac/ax mixed mode

## Reverse Engineering Process

### 1. WiFi MAC Register Discovery

The ESP32-C5 WiFi MAC registers are undocumented by Espressif. We located them through:

1. **Disassembly of the proprietary blob** (`libpp.a`) using `riscv32-esp-elf-objdump`
2. **Function name extraction** via `riscv32-esp-elf-nm` — found `hal_mac_set_csi`, `hal_mac_set_csi_filter`, `hal_mac_init`, etc.
3. **Register address extraction** from `lui` (Load Upper Immediate) instructions in the disassembled functions

Key discovery from `hal_mac_set_csi` disassembly:
```asm
00000000 <hal_mac_set_csi>:
   c:   600a47b7        lui     a5,0x600a4
  10:   09878793        addi    a5,a5,152    # 0x600A4098
```

This revealed **`0x600A4098`** as the CSI enable register and **`0x600A4000`** as the WiFi MAC base address.

### 2. Full Register Dump

We dumped all non-zero registers in the modem region (`0x600A0000–0x600AFFFF`) from a live ESP32-C5 with WiFi active. Found **1,241 non-zero registers** — the complete WiFi radio register map.

See [`dumps/full_register_dump.txt`](dumps/full_register_dump.txt) for the raw data.

### 3. Register Map (Partial)

| Address | Name | Description |
|---------|------|-------------|
| `0x600A4000` | `WIFI_MAC_BASE` | WiFi MAC register block start |
| `0x600A4080` | `RX_CTRL` | RX DMA control |
| `0x600A4084` | `RX_DMA_BASE` | RX DMA descriptor base pointer |
| `0x600A4098` | `CSI_ENABLE` | CSI master enable (bit 23) + mode control |
| `0x600A409C` | **`CSI_MODE`** | **CSI acquisition mode — the unlock register** |
| `0x600A40A0` | `CHAN_CONFIG` | Channel/frequency configuration |
| `0x600A40A4` | `TIMING_0` | Timing parameter (value 0x188 = 392) |
| `0x600A411C` | `CSI_FILTER` | CSI frame type filter (HT/VHT/HE bits) |
| `0x600A4CA8` | `MAC_CTRL` | MAC control register (init/deinit) |
| `0x600A4C8C` | `TXRX_INIT` | TX/RX initialization control |
| `0x600AD800` | `MODEM_PWR` | Modem power status |

### 4. Systematic Bit-Banging

We tested all values `0x00–0x3E` (step 2) for register `0x600A409C`, measuring the CSI subcarrier count for each. Results:

- **Values with bit 3 set AND bit 4 clear** → 245 subcarriers (HE20)
- **Values with bit 4 set** → 53 subcarriers (HT20, default)
- **All other values** → 53 subcarriers

We also tested register `0x600A40A0` (channel config) in combination with HE20 mode, attempting to enable HE40/HE80/HE160 CSI. No combination produced more than 245 subcarriers. This confirms **HE20 is the hardware ceiling for CSI capture** on ESP32-C5.

### 5. HE40/HE80/HE160 — Not Achievable

Extensive testing of register combinations confirmed that wider-bandwidth CSI modes are not available on ESP32-C5:

- Modifying `0x600A40A4` (suspected subcarrier count register) to HE40 (482) / HE80 (996) values → no effect
- Modifying `0x600A40A0` BW bits in combination with HE20 mode → no additional subcarriers
- Setting ESP-IDF bandwidth to `WIFI_BW80` / `WIFI_BW160` → CSI still reports 245 max

The CSI capture hardware appears to have a fixed 20 MHz RF frontend for channel estimation, regardless of the connection bandwidth.

## Project Structure

```
esp32c5-csi-unlock/
├── README.md                              # This file
├── LICENSE                                # MIT License
├── firmware/
│   ├── main/
│   │   ├── csi_unlock_demo.c             # Clean demo firmware (HE20 unlock)
│   │   ├── raw_mac.h                     # Reverse-engineered register definitions
│   │   ├── wraith_node_cbw40_patch.c     # HT40 PHY patch via linker wrap
│   │   ├── wraith_node_bw80_patch.c      # BW80 attempt (falls back to HT40)
│   │   ├── CMakeLists.txt                # Standard component config
│   │   └── CMakeLists_wrap.txt           # Component config with --wrap flags
│   ├── CMakeLists.txt                     # ESP-IDF project config
│   └── sdkconfig.defaults                # Build defaults for ESP32-C5
├── releases/
│   ├── esp32c5_csi_unlock.bin            # Pre-built firmware binary
│   ├── bootloader.bin                    # Bootloader binary
│   ├── partition-table.bin               # Partition table
│   └── FLASH_INSTRUCTIONS.md            # Flash without building
├── tools/
│   ├── reg_dump.c                        # Register dump utility
│   └── bw_experiment.c                   # Bandwidth experiment firmware
├── dumps/
│   ├── full_register_dump.txt            # 1,241 non-zero registers from live chip
│   ├── bw_experiment_1.txt               # CSI mode sweep (CBW20 PHY)
│   ├── bw_experiment_2.txt               # A0+9C combination results
│   ├── bw_experiment_3_phy_bb.txt        # PHY baseband register experiments
│   ├── bw_experiment_4_cbw_phy.txt       # CBW implementation + wrap tests
│   └── bw_experiment5_ht40.txt           # CSI mode sweep (HT40 PHY)
└── docs/
    └── register_map.md                   # Detailed register documentation
```

## Building & Flashing

Requires ESP-IDF v5.4+ with ESP32-C5 preview support.

```bash
# Setup
export IDF_PATH=~/esp-idf
source $IDF_PATH/export.sh

# Build
cd firmware
idf.py set-target esp32c5
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage in Your Project

Add the unlock to any ESP32-C5 CSI project — call it after `esp_wifi_set_csi(true)`:

```c
#include "raw_mac.h"

// Standard ESP-IDF CSI setup
wifi_csi_config_t csi_cfg = {
    .enable = 1,
    .acquire_csi_legacy = 1,
    .acquire_csi_ht20 = 1,
    .acquire_csi_ht40 = 1,
    .acquire_csi_su = 1,
    .acquire_csi_mu = 1,
};
esp_wifi_set_csi_config(&csi_cfg);
esp_wifi_set_csi_rx_cb(my_csi_callback, NULL);
esp_wifi_set_csi(true);

// === UNLOCK HE20 CSI ===
REG32(0x600A409C) = 0x0A;
// CSI callback now receives 245 subcarriers instead of 53
```

Your CSI callback will receive frames with `info->len / 2 = 245` subcarriers at 78.125 kHz spacing.

## Applications

245 subcarriers at 78.125 kHz spacing enables:

- **WiFi CSI sensing** with 4.7× better frequency resolution
- **Channel Impulse Response** with finer delay estimation
- **WiFi radar / SAR** with improved range profile
- **Indoor positioning** with more multipath components resolved
- **Gesture recognition** with richer feature vectors
- **Breathing / vital sign detection** with higher SNR

## Bonus: HT40 PHY Bandwidth Patch

By default, the ESP32-C5 negotiates CBW20 (20 MHz) with the access point. We successfully patched the proprietary blob to force **HT40 (40 MHz)** PHY bandwidth using linker `--wrap`:

```
Before: wifi: phytype:CBW20-SGI, snr:54, maxRate:172
After:  wifi: phytype:HT40-SGI, snr:52, maxRate:172
```

### How to Apply

Add to your `CMakeLists.txt`:

```cmake
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=hal_mac_set_csi_cbw"
    "-Wl,--wrap=ieee80211_set_phy_bw"
)
```

Add wrap functions:

```c
// Prevent blob from resetting CSI mode
extern void __real_hal_mac_set_csi_cbw(uint8_t cbw);
void __wrap_hal_mac_set_csi_cbw(uint8_t cbw) {
    *(volatile uint32_t *)0x600A409C = 0x0A;  // Force HE20 always
}

// Force HT40 PHY bandwidth
extern void __real_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi);
void __wrap_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi) {
    __real_ieee80211_set_phy_bw(iface, 2, sgi);  // 2 = BW40
}
```

### Why This Matters

With HT40, the ESP32-C5 receives 40 MHz frames from the router. While the CSI capture is still limited to the primary 20 MHz, the HT40 connection provides:
- Better throughput for data transmission
- Access to secondary channel information in `rx_ctrl`
- Foundation for future CSI bandwidth unlocks if Espressif implements `hal_mac_set_csi_cbw`

### Max TX Power

```c
esp_wifi_set_max_tx_power(84);  // Request 21 dBm
// Hardware caps at 18 dBm (72 units) on ESP32-C5
```

## Confirmed Hardware Limits

| Parameter | Default | After Patch | Silicon Limit |
|-----------|---------|-------------|---------------|
| CSI subcarriers | 53 (HT20) | **245 (HE20)** | 245 — CSI frontend fixed at 20 MHz |
| PHY bandwidth | CBW20 | **HT40** | HT40 — BW80 falls back to HT40 |
| TX power | ~13 dBm | **18 dBm** | 18 dBm (72 units) |
| MIMO streams | 1×1 | 1×1 | 1×1 — single antenna |
| Max PHY rate | 172 Mbps | 172 Mbps | HE20 MCS11 1SS |

## Why HE40+ Fails

We conducted **5 rounds** of systematic register experiments (**150+ configurations** tested) to attempt HE40 (484 sub), HE80 (996 sub), and HE160 (1992 sub) CSI capture. All failed. Here's why:

### Root Cause: Fixed 20 MHz CSI Frontend

Even after successfully patching PHY to HT40, the CSI capture engine only processes the primary 20 MHz of the channel:

```
wifi: phytype:HT40-SGI     ← PHY is 40 MHz (patched!)
CSI: nsub=245               ← but CSI still captures only 20 MHz
```

The CSI channel estimation block has a **fixed 20 MHz RF frontend hardwired in silicon**. No register configuration can change this.

### What We Tried

| Round | Experiment | Registers Modified | Result |
|-------|-----------|-------------------|--------|
| 1 | CSI mode register sweep | `0x600A409C` (all values 0x00–0x3E) | Max 245 sub (HE20) |
| 1 | Channel config + CSI mode | `0x600A40A0` + `0x600A409C` | Max 245 sub |
| 1 | A4 subcarrier count | `0x600A40A4` = 482/996/1992 | No effect |
| 1 | CSI filter all bits | `0x600A411C` bits 0–15 | No effect |
| 2 | A0 BW bits + HE20 | `0x600A40A0` bit sweep + `0x600A409C` | Max 245 sub |
| 2 | A0 upper byte | Channel-related byte modifications | Max 245 sub |
| 3 | BB channel BW | `0x600A4400` (from `phy_bb_cbw_chan_cfg`) | No effect on CSI |
| 3 | Digital BW40 | `0x600A9C18` (from `phy_bb_bss_cbw40_dig`) | No effect on CSI |
| 3 | BW selector | `0x600A0874` (from `phy_wifi_fbw_sel`) | No effect on CSI |
| 3 | Combined BW40/BW80 | All BW registers + HE20 mode | Max 245 sub |
| 4 | CBW implementation | Own `hal_mac_set_csi_cbw()` via wrap | Max 245 sub |
| 4 | CSI area scan | `0x600A40E0–0x600A40F8` bit sweep | Max 245 sub |
| 4 | Filter context | `0x600A42A4` bit sweep | No effect |
| 5 | PHY BW40 patch | `--wrap ieee80211_set_phy_bw` bw=2 | **PHY→HT40**, CSI still 245 |
| 5 | PHY BW80 patch | `--wrap ieee80211_set_phy_bw` bw=3 | PHY stays HT40, CSI still 245 |
| 5 | CSI mode sweep on HT40 | `0x600A409C` full sweep with HT40 PHY | Max 245 sub |
| 5 | Max TX power | `esp_wifi_set_max_tx_power(84)` | 18 dBm (capped by hardware) |

Full experiment logs: [`dumps/`](dumps/)

### The Empty Stub

Disassembly of the proprietary blob reveals that `hal_mac_set_csi_cbw` exists but is an **empty function**:

```asm
00000000 <hal_mac_set_csi_cbw>:
   0:   8082    ret
```

Espressif defined the interface for CSI channel bandwidth control but **never implemented it**. The blob calls this function with values 0 and 16 during WiFi operation, but the empty body means the calls do nothing.

### Blob Reset Prevention

The blob calls `hal_mac_set_csi_cbw(0)` during WiFi operation, which would reset the CSI mode register. Our `--wrap` intercept catches every call and forces HE20 mode, preventing the blob from overriding our unlock.

### Conclusion

The 245 subcarrier (HE20) limit is enforced at **three levels**:
1. **CSI capture hardware** — 20 MHz RF frontend for channel estimation (silicon)
2. **PHY** — maximum HT40 bandwidth (BW80 attempt falls back to HT40)
3. **Firmware blob** — `hal_mac_set_csi_cbw` is unimplemented (empty stub)

To achieve HE40+ CSI on ESP32-C5, Espressif would need to:
- Implement `hal_mac_set_csi_cbw` in the blob
- Enable wider CSI capture in the MAC hardware (may require silicon revision)
- Support CBW80/160 in STA mode (currently caps at HT40)

## Related Work

- [esp32-open-mac](https://github.com/esp32-open-mac/esp32-open-mac) — Open-source WiFi MAC for classic ESP32 (Xtensa). Our register discovery methodology builds on their pioneering work.
- [ESP32-C3 WiFi Driver RE](https://arxiv.org/html/2501.17684v3) — Academic reverse engineering of ESP32-C3 WiFi drivers.
- [Holl & Reinhard (2017)](https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.118.183901) — WiFi holography using CSI, published in Physical Review Letters.

## License

MIT — see [LICENSE](LICENSE).

## Disclaimer

This project involves writing to undocumented hardware registers. While we have not observed any damage or instability, **use at your own risk**. Register values may differ across ESP32-C5 revisions. Always test on non-critical hardware first.
