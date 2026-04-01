# ESP32-C5 WiFi MAC Register Map

Reverse-engineered from `libpp.a` disassembly and live register dumps.

## Memory Regions

| Region | Start | End | Description |
|--------|-------|-----|-------------|
| MODEM0 | `0x600A0000` | `0x600A5FFF` | WiFi MAC, Baseband, RF control |
| MODEM_SYSCON | `0x600A9C00` | `0x600A9FFF` | Modem system configuration |
| MODEM1 | `0x600AC000` | `0x600ACFFF` | Extended modem registers |
| MODEM_PWR | `0x600AD000` | `0x600ADFFF` | Modem power management |
| MODEM_LPCON | `0x600AF000` | `0x600AF7FF` | Low-power modem control |
| I2C_ANA_MST | `0x600AF800` | `0x600AFFFF` | Analog master (PHY tuning) |

## WiFi MAC Registers (`0x600A4000` base)

### RX Path

| Offset | Address | Name | Description |
|--------|---------|------|-------------|
| +0x080 | `0x600A4080` | `RX_CTRL` | RX control. Bit 0 = reload DMA descriptors |
| +0x084 | `0x600A4084` | `RX_DMA_BASE` | RX DMA descriptor base pointer |
| +0x088 | `0x600A4088` | `RX_DMA_NEXT` | Next RX DMA descriptor |
| +0x08C | `0x600A408C` | `RX_DMA_LAST` | Last RX DMA descriptor |

### CSI Control

| Offset | Address | Name | Description |
|--------|---------|------|-------------|
| +0x098 | `0x600A4098` | `CSI_ENABLE` | CSI master enable. Bit 23 = CSI on/off. Default: `0x08A80101` |
| +0x09C | **`0x600A409C`** | **`CSI_MODE`** | **CSI acquisition mode. Default: `0x1A` (HT20, 53 sub). Set to `0x0A` for HE20 (245 sub).** |
| +0x0A0 | `0x600A40A0` | `CHAN_CFG` | Channel/frequency config. Default: `0x3A064000` |
| +0x0A4 | `0x600A40A4` | `TIMING_0` | Timing parameter. Default: `0x188` (decimal 392) |

### CSI Filter

| Offset | Address | Name | Description |
|--------|---------|------|-------------|
| +0x11C | `0x600A411C` | `CSI_FILTER` | CSI frame type filter. Bit 2=HT, Bit 3=VHT, Bit 4=HE. Default: `0x307C` |
| +0x120 | `0x600A4120` | `CSI_FILTER_2` | Extended filter. Default: `0x3F7E` |
| +0x124 | `0x600A4124` | `CSI_CFG_0` | Per-type CSI config. Default: `0x23006` |
| +0x128 | `0x600A4128` | `CSI_CFG_1` | Per-type CSI config. Default: `0x23006` |
| +0x12C | `0x600A412C` | `CSI_CFG_2` | Per-type CSI config. Default: `0x23006` |
| +0x130 | `0x600A4130` | `CSI_CFG_3` | Per-type CSI config. Default: `0x2301C` |

### MAC Control

| Offset | Address | Name | Description |
|--------|---------|------|-------------|
| +0xC8C | `0x600A4C8C` | `TXRX_INIT` | TX/RX initialization control |
| +0xCA8 | `0x600A4CA8` | `MAC_CTRL` | MAC master control (init/deinit) |

### Modem Power

| Offset | Address | Name | Description |
|--------|---------|------|-------------|
| — | `0x600AD800` | `MODEM_PWR_STATUS` | Modem power/status register |
| — | `0x600AD810` | `MODEM_PWR_CTRL` | Modem power control |

## CSI_MODE Register (0x600A409C) — Bit Analysis

```
Bit 4: Force HT20 mode (when set, CSI is always 53 subcarriers)
Bit 3: Enable HE capture (required for 245 subcarriers)
Bit 2: Unknown (not required for HE20)
Bit 1: Enable extended CSI (required for 245 subcarriers)
Bit 0: Unknown
```

### Tested Values

| Value | Binary | Result |
|-------|--------|--------|
| `0x00` | `000000` | 53 sub (no mode selected) |
| `0x02` | `000010` | 53 sub |
| `0x04` | `000100` | 53 sub |
| `0x06` | `000110` | 53 sub |
| `0x08` | `001000` | 53 sub (bit 3 alone insufficient) |
| **`0x0A`** | **`001010`** | **245 sub** (bit 3 + bit 1) |
| **`0x0C`** | **`001100`** | **245 sub** (bit 3 + bit 2) |
| **`0x0E`** | **`001110`** | **245 sub** (bit 3 + bit 2 + bit 1) |
| `0x10` | `010000` | 53 sub |
| `0x1A` | `011010` | 53 sub (DEFAULT — bit 4 forces HT20) |
| **`0x3C`** | **`111100`** | **245 sub** |
| **`0x3E`** | **`111110`** | **245 sub** |

## Source Functions (from `libpp.a` disassembly)

| Function | Registers Accessed |
|----------|--------------------|
| `hal_mac_set_csi` | `0x600A4098` (CSI enable, bit 23) |
| `hal_mac_set_csi_filter` | `0x600A4098`, `0x600A411C` (filter bits) |
| `hal_mac_init` | `0x600A4CA8` (MAC control) |
| `hal_mac_rx_set_base` | `0x600A4084` (RX DMA base) |
| `mac_txrx_init` | `0x600A4C8C` (TXRX init) |

## Comparison with Other ESP32 Variants

| Chip | WiFi MAC Base | CSI Mode Register | Max Subcarriers |
|------|---------------|-------------------|-----------------|
| ESP32 (Xtensa) | `0x3FF73000` | Unknown | 52 (HT20) |
| ESP32-C3 (RISC-V) | `0x60033000` | Unknown | 52 (HT20) |
| **ESP32-C5 (RISC-V)** | **`0x600A4000`** | **`0x600A409C`** | **245 (HE20)** |
