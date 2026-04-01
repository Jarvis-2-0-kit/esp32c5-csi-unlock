/**
 * ESP32-C5 HE20 CSI Unlock Demo
 *
 * Demonstrates unlocking 245-subcarrier HE20 CSI on ESP32-C5
 * via direct register write to 0x600A409C.
 *
 * Default ESP-IDF CSI: 53 subcarriers (HT20, 312.5 kHz spacing)
 * After unlock:       245 subcarriers (HE20, 78.125 kHz spacing)
 *
 * Usage: flash to any ESP32-C5 board, monitor serial output.
 * The CSI callback prints subcarrier count for each received frame.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "raw_mac.h"

static const char *TAG = "CSI_UNLOCK";

/* ---- CONFIGURE THESE ---- */
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASS       "YOUR_PASSWORD"
/* ------------------------- */

static EventGroupHandle_t s_wifi_events;
#define WIFI_CONNECTED_BIT BIT0

static volatile uint32_t s_frame_count = 0;
static volatile uint16_t s_last_nsub = 0;

static void csi_callback(void *ctx, wifi_csi_info_t *info)
{
    if (!info || !info->buf || info->len < 4) return;
    uint16_t n_sub = info->len / 2;
    s_last_nsub = n_sub;
    s_frame_count++;

    /* Print every 50th frame to avoid flooding */
    if (s_frame_count % 50 == 0) {
        ESP_LOGI(TAG, "CSI frame #%lu: %d subcarriers, RSSI=%d dBm, ch=%d",
                 s_frame_count, n_sub, info->rx_ctrl.rssi, info->rx_ctrl.channel);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START)
            esp_wifi_connect();
        else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Connected — IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "  ESP32-C5 CSI Unlock — HE20 245 Subcarriers");
    ESP_LOGI(TAG, "==============================================");

    /* ---- WiFi Init ---- */
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t sta_cfg = {0};
    memcpy(sta_cfg.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(sta_cfg.sta.password, WIFI_PASS, strlen(WIFI_PASS));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    /* Request 5 GHz band */
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);

    ESP_ERROR_CHECK(esp_wifi_start());

    /* Wait for connection */
    ESP_LOGI(TAG, "Connecting to WiFi...");
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    /* ---- Standard CSI Setup ---- */
    wifi_csi_config_t csi_cfg = {
        .enable              = 1,
        .acquire_csi_legacy  = 1,
        .acquire_csi_ht20    = 1,
        .acquire_csi_ht40    = 1,
        .acquire_csi_su      = 1,
        .acquire_csi_mu      = 1,
        .acquire_csi_dcm     = 0,
        .acquire_csi_beamformed = 0,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    /* Collect baseline (HT20) */
    ESP_LOGI(TAG, "Collecting HT20 baseline (3 seconds)...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "HT20 baseline: %d subcarriers, %lu frames",
             s_last_nsub, s_frame_count);

    /* ---- THE UNLOCK ---- */
    ESP_LOGW(TAG, "");
    ESP_LOGW(TAG, ">>> Applying HE20 CSI unlock: REG 0x600A409C = 0x0A <<<");
    ESP_LOGW(TAG, "");

    uint32_t reg_before = REG32(0x600A409C);
    REG32(0x600A409C) = 0x0A;
    uint32_t reg_after = REG32(0x600A409C);

    ESP_LOGW(TAG, "Register 0x600A409C: 0x%08lX -> 0x%08lX", reg_before, reg_after);

    /* Collect post-unlock */
    s_frame_count = 0;
    s_last_nsub = 0;
    ESP_LOGI(TAG, "Collecting HE20 data (3 seconds)...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGW(TAG, "HE20 result: %d subcarriers, %lu frames", s_last_nsub, s_frame_count);

    if (s_last_nsub > 100) {
        ESP_LOGW(TAG, "");
        ESP_LOGW(TAG, "*** SUCCESS: HE20 CSI unlocked! %d subcarriers ***", s_last_nsub);
        ESP_LOGW(TAG, "*** 4.7x improvement over default HT20 (53 sub)  ***");
    } else {
        ESP_LOGE(TAG, "Unlock did not increase subcarriers. "
                 "Ensure router supports 802.11ax (WiFi 6) on 5 GHz.");
    }

    /* ---- Monitor loop ---- */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "CSI: %d subcarriers | frames: %lu",
                 s_last_nsub, s_frame_count);
    }
}
