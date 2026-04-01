// ESP32-C5 WiFi MAC Raw Register Access
// Reverse engineered from hal_mac_* disassembly
#pragma once
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// === WiFi MAC registers (base 0x600A4000) ===
#define WIFI_MAC_BASE           0x600A4000

// RX DMA
#define WIFI_RX_DMA_BASE        REG32(0x600A4084)  // hal_mac_rx_set_base
#define WIFI_RX_DMA_NEXT        REG32(0x600A4088)
#define WIFI_RX_DMA_LAST        REG32(0x600A408C)
#define WIFI_RX_CTRL            REG32(0x600A4080)  // bit 0 = reload descriptors

// CSI control
#define WIFI_CSI_ENABLE         REG32(0x600A4098)  // bit 23 = CSI on/off
#define WIFI_CSI_FILTER         REG32(0x600A411C)  // bit 2 = HT, bit 3 = VHT, bit 4 = HE
#define WIFI_CSI_ENABLE_BIT     (1 << 23)
#define WIFI_CSI_HT_BIT         (1 << 2)
#define WIFI_CSI_VHT_BIT        (1 << 3)
#define WIFI_CSI_HE_BIT         (1 << 4)

// MAC control
#define WIFI_MAC_CTRL           REG32(0x600A4CA8)  // hal_mac_init / deinit
#define WIFI_TXRX_INIT          REG32(0x600A4C8C)  // mac_txrx_init

// DMA interrupt
#define WIFI_DMA_INT_STATUS_C5  REG32(0x600A4C48)  // estimated from ESP32 offset
#define WIFI_DMA_INT_CLR_C5     REG32(0x600A4C4C)

// MAC address filter
#define WIFI_MAC_ADDR_0         REG32(0x600A4040)
#define WIFI_MAC_ADDR_0_HI      REG32(0x600A4044)

// BSSID filter
#define WIFI_BSSID_FILTER_0     REG32(0x600A4000)

// TX slots (5 slots, estimated from ESP32 layout)
#define WIFI_TX_PLCP0_BASE      0x600A4D20  // estimated
#define WIFI_TX_CONFIG_BASE     0x600A4D1C  // estimated

// Modem power
#define MODEM_PWR_STATUS        REG32(0x600AD800)
#define MODEM_PWR_CTRL          REG32(0x600AD810)

// === PHY/Baseband functions (called via blob) ===
// phy_bb_cbw_chan_cfg — channel bandwidth config
// phy_bb_bss_cbw40_dig — 40 MHz digital baseband
// phy_enable_agc / phy_disable_agc — AGC control

// === Utility macros ===
#define WIFI_CSI_IS_ENABLED()    (WIFI_CSI_ENABLE & WIFI_CSI_ENABLE_BIT)
#define WIFI_CSI_SET_ENABLED(e)  do { \
    if (e) WIFI_CSI_ENABLE |= WIFI_CSI_ENABLE_BIT; \
    else   WIFI_CSI_ENABLE &= ~WIFI_CSI_ENABLE_BIT; \
} while(0)

// Enable all CSI types (HT + VHT + HE)
#define WIFI_CSI_ENABLE_ALL_TYPES() do { \
    WIFI_CSI_FILTER |= (WIFI_CSI_HT_BIT | WIFI_CSI_VHT_BIT | WIFI_CSI_HE_BIT); \
} while(0)
