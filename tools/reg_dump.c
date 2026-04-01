/**
 * ESP32-C5 WiFi/Modem Register Dump Utility
 *
 * Dumps all non-zero registers in the modem region (0x600A0000–0x600AFFFF)
 * after WiFi initialization. Output via serial at 115200 baud.
 *
 * Build: idf.py --preview build
 * Flash: idf.py --preview -p PORT flash monitor
 */

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "REGDUMP"

static void dump_region(const char *name, uint32_t base, uint32_t end)
{
    ESP_LOGI(TAG, "=== %s: 0x%08lX - 0x%08lX ===", name, base, end);
    for (uint32_t addr = base; addr <= end; addr += 4) {
        volatile uint32_t *reg = (volatile uint32_t *)addr;
        uint32_t val = *reg;
        if (val != 0) {
            printf("0x%08lX = 0x%08lX\n", addr, val);
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started. Dumping modem registers...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    dump_region("MODEM0 (WiFi MAC/BB)", 0x600A0000, 0x600A0FFF);
    dump_region("MODEM0+0x1000",        0x600A1000, 0x600A1FFF);
    dump_region("MODEM0+0x2000",        0x600A2000, 0x600A2FFF);
    dump_region("MODEM0+0x3000",        0x600A3000, 0x600A3FFF);
    dump_region("MODEM0+0x4000 (MAC)",  0x600A4000, 0x600A4FFF);
    dump_region("MODEM0+0x5000",        0x600A5000, 0x600A5FFF);
    dump_region("MODEM_SYSCON",         0x600A9C00, 0x600A9FFF);
    dump_region("MODEM1",               0x600AC000, 0x600ACFFF);
    dump_region("MODEM_PWR",            0x600AD000, 0x600ADFFF);
    dump_region("MODEM_LPCON",          0x600AF000, 0x600AF7FF);
    dump_region("I2C_ANA_MST",          0x600AF800, 0x600AFFFF);

    ESP_LOGI(TAG, "=== DUMP COMPLETE ===");

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
