/**
 * ESP32-C5 CSI Bandwidth Experiment
 *
 * Systematically tests register values to find configurations
 * that increase CSI subcarrier count beyond the default 53.
 *
 * This firmware identified register 0x600A409C as the CSI mode
 * control, with value 0x0A enabling HE20 (245 subcarriers).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#define TAG "BW_EXP"
#define REG32(addr) (*(volatile uint32_t *)(addr))

static EventGroupHandle_t s_wifi_events;
#define WIFI_CONNECTED_BIT BIT0
static volatile uint16_t g_last_nsub = 0;
static volatile uint32_t g_csi_count = 0;

static void csi_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf || info->len < 4) return;
    g_last_nsub = info->len / 2;
    g_csi_count++;
}

static void test_register(uint32_t addr, uint32_t val, const char *desc) {
    uint32_t orig = REG32(addr);
    g_csi_count = 0;
    g_last_nsub = 0;
    REG32(addr) = val;
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGW(TAG, "%s | 0x%08lX = 0x%08lX -> 0x%08lX | n_sub=%d cnt=%lu",
             desc, addr, orig, val, g_last_nsub, g_csi_count);
    REG32(addr) = orig;
    vTaskDelay(pdMS_TO_TICKS(500));
}

static void wifi_evt(void *a, esp_event_base_t b, int32_t id, void *d) {
    if (b == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) esp_wifi_connect();
        else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
            esp_wifi_connect();
        }
    } else if (b == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t c = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&c));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL);

    /* CONFIGURE YOUR WIFI HERE */
    wifi_config_t sc = {0};
    memcpy(sc.sta.ssid, "YOUR_SSID", 9);
    memcpy(sc.sta.password, "YOUR_PASSWORD", 13);
    sc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sc));
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    wifi_csi_config_t cc = {
        .enable=1, .acquire_csi_legacy=1, .acquire_csi_ht20=1,
        .acquire_csi_ht40=1, .acquire_csi_su=1, .acquire_csi_mu=1,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&cc));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGW(TAG, "Baseline: n_sub=%d", g_last_nsub);

    ESP_LOGW(TAG, "=== 0x600A409C sweep ===");
    for (uint32_t v = 0; v <= 0x3E; v += 2) {
        char d[32];
        snprintf(d, sizeof(d), "9C=0x%02lX", v);
        test_register(0x600A409C, v, d);
    }

    ESP_LOGW(TAG, "=== DONE ===");
    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
