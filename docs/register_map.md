# ESP32-C5 WiFi/Radio Register Map — Complete Reference

> First public documentation of ESP32-C5 WiFi MAC, baseband, RF, and modem registers.
> Reverse-engineered from `libpp.a` / `libphy.a` disassembly and live register dumps.

## Overview

The ESP32-C5 WiFi/radio subsystem occupies the memory region `0x600A0000–0x600AFFFF` (64 KB). We dumped **1,239 non-zero registers** from a live chip with WiFi active, distributed across 5 major blocks:

| Block | Address Range | Registers | Description |
|-------|--------------|-----------|-------------|
| **Baseband (BB)** | `0x600A0000–0x600A0FFF` | 543 | PHY signal processing, channel estimation, AGC, equalizer |
| **RF Control** | `0x600A1000–0x600A1FFF` | 31 | RF frontend, gain tables, LNA, PA, calibration |
| **WiFi MAC** | `0x600A4000–0x600A4FFF` | 168 | MAC layer, DMA, CSI, TX/RX, filtering, beacon |
| **Modem SYSCON** | `0x600A9C00–0x600A9FFF` | 112 | Clock gating, power domains, antenna, debug |
| **Modem LPCON + I2C ANA** | `0x600AF000–0x600AFFFF` | 385 | Low-power control, sleep/wake, PLL, analog tuning |

## 1. Baseband (BB) — `0x600A0000–0x600A0FFF`

543 registers. The largest block — contains the entire PHY digital signal processing chain.

### Known Functions (from `libphy.a` disassembly)

| Function | Register(s) | Purpose |
|----------|-------------|---------|
| `phy_bb_cbw_chan_cfg` | `0x600A4400` | Channel bandwidth configuration |
| `phy_bb_bss_cbw40_dig` | `0x600A9C18` | Digital BW40 enable (bit 2) |
| `phy_bb_reg_init_new` | `0x600A0xxx` (multiple) | Baseband initialization |
| `phy_bb_txpwr_init` | `0x600A0xxx` | TX power calibration tables |
| `phy_bb_fsm_rst` | Unknown | Baseband FSM reset |
| `phy_bb_gain_index` | Unknown | RX gain index configuration |
| `phy_bb_wdg_cfg` | Unknown | Baseband watchdog |
| `bb_agc_reg_update` | Unknown | AGC register update |

### BB Register Structure

The BB registers appear mirrored — addresses `0x600A0000–0x600A007F` have identical values to `0x600A0080–0x600A00FF`. This suggests two identical processing chains (possibly for 2.4 GHz and 5 GHz bands).

Sample registers:
```
0x600A0000 = 0x00000007    # BB control/enable
0x600A0008 = 0x00010032    # BB mode config
0x600A000C = 0x2EC00000    # BB frequency config
0x600A0014 = 0x78F578F0    # BB timing parameters
0x600A0018 = 0x000003E2    # BB filter config
0x600A001C = 0x74201E0C    # BB AGC parameters
```

## 2. RF Control — `0x600A1000–0x600A1FFF`

31 registers. Controls the RF analog frontend.

### Known Functions

| Function | Register(s) | Purpose |
|----------|-------------|---------|
| `phy_wifi_fbw_sel` | `0x600A0874` | Filter bandwidth selector (bit 17 = 20kHz, bit 20 = wider) |
| `phy_rfrx_gain_cal` | `0x600A1xxx` | RX gain calibration |
| `phy_rfrx_gain_index` | `0x600A1xxx` | RX gain table index |
| `phy_rx_gain_force` | `0x600A1xxx` | Force specific RX gain |
| `phy_set_rx_gain_table` | `0x600A1xxx` | Load RX gain table |
| `phy_txpwr_cal_track_new` | `0x600A1xxx` | TX power tracking calibration |

### RF Register Ranges

```
0x600A1510–0x600A1528  — RF configuration (7 registers)
0x600A1918–0x600A1924  — RF calibration (4 registers)
0x600A1B00–0x600A1B34  — RF gain tables (14 registers)
0x600A1FFC             — RF version/ID register (0x02408090)
```

## 3. WiFi MAC — `0x600A4000–0x600A4FFF`

168 registers. The core MAC layer — DMA, CSI, TX/RX, filtering.

### 3.1 RX Path (`0x600A4000–0x600A407F`)

| Address | Name | Bits | Description |
|---------|------|------|-------------|
| `0x600A4000` | `MAC_ADDR_0` | [31:0] | MAC address bytes 0-3 (station interface 0) |
| `0x600A4004` | `MAC_ADDR_0_HI` | [15:0] | MAC address bytes 4-5 |
| `0x600A400C` | `BSSID_FILTER` | [31:0] | BSSID filter control |
| `0x600A4040` | `MAC_ADDR_SLOT_0` | [31:0] | MAC address filter slot 0 |
| `0x600A4064` | `ACK_ENABLE_0` | [16] | ACK enable for slot 0 (bit 16) |

### 3.2 RX DMA (`0x600A4080–0x600A408F`)

| Address | Name | R/W | Description |
|---------|------|-----|-------------|
| `0x600A4080` | `RX_CTRL` | R/W | RX control. Bit 0 = reload DMA descriptors. Bit 31 = AP mode flag. |
| `0x600A4084` | `RX_DMA_BASE` | R/W | RX DMA linked-list base pointer (written by `hal_mac_rx_set_base`) |
| `0x600A4088` | `RX_DMA_NEXT` | R | Next RX DMA descriptor address |
| `0x600A408C` | `RX_DMA_LAST` | R | Last RX DMA descriptor address |

### 3.3 CSI Control (`0x600A4090–0x600A40CF`)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| `0x600A4090` | `CSI_TIMING_0` | `0x00027428` | CSI timing parameter 0 |
| `0x600A4094` | `CSI_TIMING_1` | `0x00027428` | CSI timing parameter 1 (mirror) |
| **`0x600A4098`** | **`CSI_ENABLE`** | **`0x08A80101`** | **CSI master enable. Bit 23 = on/off.** |
| **`0x600A409C`** | **`CSI_MODE`** | **`0x1A`** | **CSI acquisition mode. `0x0A` = HE20 (245 sub). THE UNLOCK REGISTER.** |
| `0x600A40A0` | `CHAN_CONFIG` | `0x3A064000` | Channel/frequency configuration |
| `0x600A40A4` | `CSI_PARAM_0` | `0x00000188` | CSI parameter (decimal 392) |
| `0x600A40A8` | `CSI_PARAM_1` | `0x426B426B` | CSI parameter (repeated pattern) |
| `0x600A40AC` | `CSI_PARAM_2` | `0xD20B0188` | CSI parameter |
| `0x600A40B8` | `CSI_CTRL_2` | `0x00000080` | CSI control (bit 7) |
| `0x600A40BC` | `CSI_MASK_0` | `0xFFFFFFFF` | CSI subcarrier mask 0 |
| `0x600A40C0` | `CSI_MASK_1` | `0xEBC4FFFF` | CSI subcarrier mask 1 |

### 3.4 CSI Timing / Power (`0x600A40D0–0x600A40FF`)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| `0x600A40D8` | `CSI_PWR_0` | `0x0001C387` | CSI power threshold 0 |
| `0x600A40DC` | `CSI_PWR_1` | `0x0001C385` | CSI power threshold 1 |
| `0x600A40E0` | `CSI_PWR_2` | `0x0001C385` | CSI power threshold 2 |
| `0x600A40E4` | `CSI_PWR_3` | `0x0001C385` | CSI power threshold 3 |
| `0x600A40E8` | `CSI_GAIN_0` | `0x07860000` | CSI gain parameter 0 |
| `0x600A40EC` | `CSI_GAIN_1` | `0x07860000` | CSI gain parameter 1 |
| `0x600A40F0` | `CSI_GAIN_2` | `0x07860000` | CSI gain parameter 2 |
| `0x600A40F4` | `CSI_GAIN_3` | `0x07860000` | CSI gain parameter 3 |
| `0x600A40F8` | `CSI_SUBCARRIER_MASK` | `0x0000FFFF` | Active subcarrier mask |

### 3.5 CSI Filter (`0x600A4110–0x600A4170`)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| `0x600A4110` | `FILTER_CTRL` | `0xA0180820` | Filter control register |
| `0x600A4114` | `FILTER_MASK` | `0x000000FF` | Frame type filter mask |
| `0x600A4118` | `FILTER_MODE` | `0x81B00000` | Filter mode config |
| **`0x600A411C`** | **`CSI_FILTER`** | **`0x0000307C`** | **CSI frame type filter. Bit 2=HT, 3=VHT, 4=HE.** |
| `0x600A4120` | `CSI_FILTER_2` | `0x00003F7E` | Extended CSI filter |
| `0x600A4124` | `CSI_TYPE_CFG_0` | `0x00023006` | Per-type CSI config |
| `0x600A4128` | `CSI_TYPE_CFG_1` | `0x00023006` | Per-type CSI config |
| `0x600A412C` | `CSI_TYPE_CFG_2` | `0x00023006` | Per-type CSI config |
| `0x600A4130` | `CSI_TYPE_CFG_3` | `0x0002301C` | Per-type CSI config (different from 0-2) |
| `0x600A4168` | `RX_FILTER_MASK_0` | `0xFFFFFFFF` | RX packet filter mask |
| `0x600A416C` | `RX_FILTER_MASK_1` | `0xFFFFFFFF` | RX packet filter mask |

### 3.6 TX Path (`0x600A4200–0x600A43FF`)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| `0x600A42A4` | `TX_CSI_CONTEXT` | `0x0000A3A8` | TX-side CSI context |
| `0x600A42B0` | `TX_SLOT_CTRL` | `0x00000060` | TX slot control |
| `0x600A42B4` | `TX_RATE_TABLE` | `0xFEFE0646` | TX rate configuration table |
| `0x600A42B8` | `TX_PLCP_CTRL` | `0x4FE3F8FE` | TX PLCP control |
| `0x600A43BC` | `TX_TIMEOUT` | `0x00000C53` | TX timeout (decimal 3155) |
| `0x600A43C8` | `TX_BEACON_CTRL` | `0x80000000` | Beacon TX control |

### 3.7 BB Channel BW / MAC Control (`0x600A4400–0x600A4FFF`)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| **`0x600A4400`** | **`BB_CHAN_BW`** | **`0xC1CB3750`** | **Baseband channel bandwidth (written by `phy_bb_cbw_chan_cfg`)** |
| `0x600A4404` | `BB_GAIN_TABLE` | `0x80808080` | Baseband gain (4 × 8-bit gain values) |
| `0x600A4408` | `BB_TIMING` | `0x00001414` | Baseband timing (2 × 8-bit values) |
| `0x600A4C48` | `DMA_INT_STATUS` | — | DMA interrupt status (read to get cause) |
| `0x600A4C4C` | `DMA_INT_CLR` | — | DMA interrupt clear (write to acknowledge) |
| **`0x600A4C8C`** | **`TXRX_INIT`** | — | **TX/RX initialization control (written by `mac_txrx_init`)** |
| **`0x600A4CA8`** | **`MAC_CTRL`** | — | **MAC master control. Written by `hal_mac_init`/`hal_mac_deinit`.** |
| `0x600A4CC4` | `TXQ_CLR_ERROR` | — | TX queue clear error status |
| `0x600A4CC8` | `TXQ_STATE_COMPLETE` | — | TX queue completion status |

## 4. Modem SYSCON — `0x600A9C00–0x600A9FFF`

112 registers. System-level modem configuration.

### Known Registers (from ESP-IDF headers)

| Address | Name | Default | Description |
|---------|------|---------|-------------|
| `0x600A9C00` | `TEST_CONF` | `0x00000100` | Test configuration, memory mode |
| `0x600A9C04` | `CLK_CONF` | `0x00201002` | Clock configuration |
| `0x600A9C0C` | `CLK_DIV` | `0x64646400` | Clock dividers |
| **`0x600A9C18`** | **`DIGITAL_BW`** | **`0x10003806`** | **Digital BW config. Bit 2 = BW40 digital enable.** |

### SYSCON Bit Fields (from ESP-IDF `modem_syscon_reg.h`)

| Bit | Name | Description |
|-----|------|-------------|
| `CLK_CONF[0]` | `CLK_WIFIBB_FO` | WiFi baseband clock force-on |
| `CLK_CONF[1]` | `CLK_WIFIMAC_FO` | WiFi MAC clock force-on |
| `CLK_CONF[2]` | `CLK_WIFI_APB_FO` | WiFi APB clock force-on |
| `CLK_CONF[23:20]` | `CLK_WIFI_ST_MAP` | WiFi clock state map |
| `TEST_CONF[2]` | `ANT_FORCE_SEL_WIFI` | Force antenna select for WiFi |
| `TEST_CONF[3-7]` | `FPGA_DEBUG_CLK*` | Debug clock switches (10/20/40/80 MHz) |

## 5. Modem LPCON — `0x600AF000–0x600AF7FF`

320 registers. Low-power modem control — sleep, wake, power sequencing.

| Range | Count | Description |
|-------|-------|-------------|
| `0x600AF008–0x600AF07C` | ~30 | Power domain enables |
| `0x600AF080–0x600AF0FF` | ~32 | Sleep timer configuration |
| `0x600AF100–0x600AF3FF` | ~192 | State machine config (sleep/wake sequences) |
| `0x600AF400–0x600AF7FF` | ~64 | Wake-up triggers, retention registers |

## 6. I2C Analog Master — `0x600AF800–0x600AF900`

65 registers. All have the same value: `0x01DD1863`. This is the I2C analog master interface used for PHY tuning — PLL configuration, LNA bias, PA calibration.

### Known Functions

| Function | Purpose |
|----------|---------|
| `phy_agc_max_gain_set` | Set maximum AGC gain |
| `phy_agc_reg_init_new` | Initialize AGC registers |
| `phy_set_rx_gain_cal_dc` | RX gain DC offset calibration |
| `phy_set_rx_gain_cal_iq` | RX gain I/Q mismatch calibration |
| `phy_txpwr_correct_new` | TX power correction |
| `phy_txpwr_cal_track_new` | TX power tracking over temperature |

## Comparison with ESP32 (Classic) and ESP32-C3

| Feature | ESP32 (Xtensa) | ESP32-C3 (RISC-V) | ESP32-C5 (RISC-V) |
|---------|----------------|--------------------|--------------------|
| MAC Base | `0x3FF73000` | `0x60033000` | **`0x600A4000`** |
| BB Base | `0x3FF6E000` | Unknown | **`0x600A0000`** |
| SYSCON | `0x3FF00000` region | Unknown | **`0x600A9C00`** |
| CSI Enable | Unknown | Unknown | **`0x600A4098` bit 23** |
| CSI Mode | Not documented | Not documented | **`0x600A409C`** |
| DMA INT | `0x3FF73C48` | Unknown | **`0x600A4C48`** (estimated) |
| MAC CTRL | `0x3FF73CB8` | Unknown | **`0x600A4CA8`** |
| WiFi MAC version | 1 | 2 | **3** (`SOC_WIFI_MAC_VERSION_NUM`) |

## Blob Function → Register Map

### `libpp.a` (WiFi MAC layer)

| Function | Registers | Description |
|----------|-----------|-------------|
| `hal_mac_set_csi` | `0x600A4098` | CSI enable (bit 23) |
| `hal_mac_set_csi_filter` | `0x600A4098`, `0x600A411C` | CSI frame type filter |
| `hal_mac_set_csi_cbw` | *(empty stub)* | CSI channel bandwidth — NOT IMPLEMENTED |
| `hal_mac_init` | `0x600A4CA8` | MAC initialization |
| `hal_mac_deinit` | `0x600A4CA8` | MAC deinitialization |
| `hal_mac_rx_set_base` | `0x600A4084` | Set RX DMA base address |
| `hal_mac_rx_set_dscr_reload` | `0x600A4080` | Trigger RX descriptor reload |
| `hal_mac_set_addr` | `0x600A4040` | Set MAC address in filter slot |
| `hal_mac_set_bssid` | `0x600A4000` | Set BSSID filter |
| `hal_mac_set_rxq_policy` | `0x600A4080` region | Set RX queue policy |
| `hal_mac_tsf_reset` | Unknown | Reset TSF timer |
| `hal_mac_get_txq_complete` | `0x600A4CC8` | Read TX queue completion status |
| `hal_mac_clr_txq_state` | `0x600A4CC4` | Clear TX queue state |
| `mac_txrx_init` | `0x600A4C8C` | TX/RX initialization |
| `hal_mac_tx_is_cbw40` | Unknown | Check if TX is using CBW40 |

### `libnet80211.a` (WiFi 802.11 stack)

| Function | Wrappable | Description |
|----------|-----------|-------------|
| `ieee80211_set_phy_bw` | **Yes** (`--wrap`) | Sets PHY bandwidth (1=BW20, 2=BW40, 3=BW80) |
| `ieee80211_update_bandwidth` | Yes | Updates connection bandwidth |
| `get_channel_max_bandwidth` | **Yes** (`--wrap`) | Returns max allowed BW for channel |
| `wifi_set_bw_process` | Yes | Processes BW change request |
| `ieee80211_is_40mhz_valid_bw` | Yes | Checks if 40 MHz is valid for channel |

### `libphy.a` (PHY layer)

| Function | Register(s) | Description |
|----------|-------------|-------------|
| `phy_bb_cbw_chan_cfg` | `0x600A4400` | Channel bandwidth config in baseband |
| `phy_bb_bss_cbw40_dig` | `0x600A9C18` | Digital BW40 enable (bit 2) |
| `phy_wifi_fbw_sel` | `0x600A0874` | Filter bandwidth selector |
| `phy_enable_agc` | Unknown | Enable Automatic Gain Control |
| `phy_disable_agc` | Unknown | Disable AGC |
| `phy_force_rx_gain` | Unknown | Force specific RX gain level |
| `phy_bb_txpwr_init` | Unknown | TX power table initialization |

## CSI_MODE Register (0x600A409C) — Complete Bit Map

```
Bit 0: Unknown
Bit 1: Extended CSI enable (required for HE20)
Bit 2: Alternative HE path
Bit 3: HE capture mode (required for HE20, combined with bit 1 or bit 2)
Bit 4: Force HT20 legacy mode (MUST BE CLEARED for HE20)
Bit 5: Unknown (works for HE20 when combined with bit 3)
Bits 6-7: Unknown
```

### Complete Test Results (CBW20 PHY)

| Value | Binary | n_sub | Notes |
|-------|--------|-------|-------|
| `0x00` | `000000` | 53 | All cleared — defaults to HT20 |
| `0x02` | `000010` | 53 | Bit 1 alone insufficient |
| `0x04` | `000100` | 53 | Bit 2 alone insufficient |
| `0x06` | `000110` | 53 | Bits 1+2 without bit 3 |
| `0x08` | `001000` | 53 | Bit 3 alone insufficient |
| **`0x0A`** | **`001010`** | **245** | **Bit 3 + bit 1 = HE20** |
| **`0x0C`** | **`001100`** | **245** | **Bit 3 + bit 2 = HE20** |
| **`0x0E`** | **`001110`** | **245** | **Bits 3+2+1 = HE20** |
| `0x10` | `010000` | 53 | Bit 4 alone |
| `0x12` | `010010` | 53 | Bit 4 overrides bit 1 |
| `0x14` | `010100` | 53 | Bit 4 overrides bit 2 |
| `0x16` | `010110` | 53 | Bit 4 overrides bits 2+1 |
| `0x18` | `011000` | 53 | Bit 4 overrides bit 3 |
| `0x1A` | `011010` | 53 | **DEFAULT** — bit 4 forces HT20 |
| `0x1C` | `011100` | 53 | Bit 4 overrides bits 3+2 |
| `0x1E` | `011110` | 53 | Bit 4 overrides all |
| `0x20`–`0x3E` | Various | 53 | Various (some give 245 on HT40 PHY) |
| **`0x3C`** | **`111100`** | **245** | Upper bits don't interfere when bit 4 clear |
| **`0x3E`** | **`111110`** | **245** | Upper bits don't interfere when bit 4 clear |

### Results with HT40 PHY (after `ieee80211_set_phy_bw` wrap)

With HT40 PHY active, more CSI mode values produce 245 subcarriers:

| Value | CBW20 Result | HT40 Result | Notes |
|-------|-------------|-------------|-------|
| `0x04` | 53 | **245** | HT40 PHY enables this value |
| `0x06` | 53 | **245** | HT40 PHY enables this value |
| `0x08` | 53 | **245** | HT40 PHY enables this value |
| `0x14` | 53 | **245** | HT40 PHY enables this value |
| `0x18` | 53 | **245** | HT40 PHY enables this value |
| `0x1A` | 53 | **245** | **DEFAULT now works on HT40!** |
| `0x1E` | 53 | **245** | HT40 PHY enables this value |
| `0x20` | 53 | **245** | HT40 PHY enables this value |
| `0x22` | 53 | **245** | HT40 PHY enables this value |

This confirms the interaction between PHY bandwidth and CSI mode — HT40 PHY relaxes the CSI mode restrictions.

## Power Management Registers

### Modem Power (`0x600AD800`)

| Address | Name | Observed Value | Description |
|---------|------|---------------|-------------|
| `0x600AD800` | `MODEM_PWR_STATUS` | Variable | Modem power state. Read by multiple `hal_mac_*` functions. |
| `0x600AD810` | `MODEM_PWR_CTRL` | Variable | Modem power control. Written during init/sleep. |

### Low-Power Control (`0x600AF000–0x600AF7FF`)

320 registers controlling sleep/wake state machine. Key areas:

| Range | Purpose |
|-------|---------|
| `0x600AF008–0x600AF018` | Power domain enable flags |
| `0x600AF080–0x600AF0FF` | Sleep timer thresholds |
| `0x600AF100–0x600AF1FF` | Sleep entry sequence |
| `0x600AF200–0x600AF2FF` | Wake-up sequence |
| `0x600AF300–0x600AF3FF` | Retention memory config |
| `0x600AF400–0x600AF7FF` | Wake-up sources and triggers |

## DMA Descriptors

The WiFi MAC uses linked-list DMA for both TX and RX. Based on esp32-open-mac analysis (adapted for C5):

```c
typedef struct dma_list_item {
    uint32_t has_data : 1;    // bit 0: descriptor has valid data
    uint32_t owner    : 1;    // bit 1: 0=software, 1=hardware
    uint32_t reserved : 6;
    uint32_t length   : 12;   // data length in bytes
    uint32_t size     : 12;   // buffer size in bytes
    uint8_t  *packet;         // pointer to data buffer
    struct dma_list_item *next; // next descriptor in chain
} dma_list_item;
```

RX chain base is set via `hal_mac_rx_set_base` → writes to `0x600A4084`.

## CSI Data Length

The CSI data length is stored in `rx_ctrl.rx_channel_estimate_len` (10-bit field, max 1023). This means the hardware CAN report up to 511 subcarriers (1022 bytes / 2 bytes per subcarrier). The 245 subcarrier limit is not a buffer constraint — it's the CSI capture frontend bandwidth.

```asm
; wdev_csi_len_align — extracts 10-bit CSI length
lbu a5, 39(a0)    ; upper 2 bits
lbu a4, 38(a0)    ; lower 8 bits
andi a5, a5, 3    ; mask to 2 bits
slli a5, a5, 8    ; shift left
or a0, a5, a4     ; combine = 10-bit value (max 1023)
```
