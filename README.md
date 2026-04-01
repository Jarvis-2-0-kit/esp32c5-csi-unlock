# ESP32-C5 CSI Unlock — HE20 245 Subcarriers via Raw Register Access

**First documented reverse engineering of ESP32-C5 WiFi MAC registers to unlock HE20 CSI mode (245 subcarriers vs default 53).**

The ESP32-C5 is Espressif's first dual-band WiFi 6 (802.11ax) RISC-V microcontroller. While the ESP-IDF CSI API exposes only HT20 CSI data (53 subcarriers), the hardware is capable of capturing HE20 (High Efficiency) CSI with **245 subcarriers** — a **4.7× improvement** in frequency resolution.

This project documents the reverse engineering process, provides the register map, and includes ready-to-flash firmware that unlocks HE20 CSI on any ESP32-C5 board.

## Results

| Mode | Subcarriers | Spacing | Effective BW | Method |
|------|-------------|---------|-------------|--------|
| HT20 (default) | 53 | 312.5 kHz | 16.6 MHz | ESP-IDF API |
| **HE20 (unlocked)** | **245** | **78.125 kHz** | **19.1 MHz** | **Register 0x600A409C = 0x0A** |
| HE40 (attempted) | N/A | — | — | Not achievable via registers |
| HE80 (attempted) | N/A | — | — | Not achievable via registers |

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
├── README.md                    # This file
├── LICENSE                      # MIT License
├── firmware/
│   ├── main/
│   │   ├── wraith_node.c       # Firmware with HE20 CSI unlock
│   │   ├── raw_mac.h           # Reverse-engineered register definitions
│   │   └── CMakeLists.txt      # ESP-IDF component config
│   ├── CMakeLists.txt           # ESP-IDF project config
│   └── sdkconfig.defaults      # Build defaults for ESP32-C5
├── tools/
│   ├── reg_dump.c              # Register dump utility
│   └── bw_experiment.c         # Bandwidth experiment firmware
├── dumps/
│   ├── full_register_dump.txt  # 1,241 non-zero registers from live chip
│   ├── bw_experiment_1.txt     # 0x600A409C sweep results
│   └── bw_experiment_2.txt     # A0+9C combination results
└── docs/
    ├── register_map.md         # Detailed register documentation
    └── disassembly_notes.md    # hal_mac_* function analysis
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

## Related Work

- [esp32-open-mac](https://github.com/esp32-open-mac/esp32-open-mac) — Open-source WiFi MAC for classic ESP32 (Xtensa). Our register discovery methodology builds on their pioneering work.
- [ESP32-C3 WiFi Driver RE](https://arxiv.org/html/2501.17684v3) — Academic reverse engineering of ESP32-C3 WiFi drivers.
- [Holl & Reinhard (2017)](https://journals.aps.org/prl/abstract/10.1103/PhysRevLett.118.183901) — WiFi holography using CSI, published in Physical Review Letters.

## License

MIT — see [LICENSE](LICENSE).

## Disclaimer

This project involves writing to undocumented hardware registers. While we have not observed any damage or instability, **use at your own risk**. Register values may differ across ESP32-C5 revisions. Always test on non-critical hardware first.
