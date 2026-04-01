// WRAITH v11 — Patch CBW via linker --wrap
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "raw_mac.h"

#define WIFI_SSID "Sunrise_3833247"
#define WIFI_PASS "3cfnSxobwN6nqvse"
#define SERVER_IP "192.168.1.33"
#define SERVER_PORT 7331
#define OTA_PORT 8032
#define CSI_PING_INTERVAL 5
#define FW_VERSION 11
#define WRAITH_MAGIC 0x48545257
#define MAX_NODES 8
#define OTA_CMD_MAGIC 0x41544F57

typedef struct __attribute__((packed)) {
    uint32_t magic; uint8_t node_id; uint8_t channel;
    int8_t rssi; int8_t noise_floor; uint32_t timestamp_us; uint16_t n_sub;
} wraith_pkt_t;

static const char *TAG = "WRAITH";
static int g_udp_sock = -1;
static struct sockaddr_in g_server;
static EventGroupHandle_t g_wifi_events;
#define WIFI_CONNECTED_BIT BIT0
static struct sockaddr_in g_ping_target;
static int g_ping_sock = -1;
static volatile bool g_ota_in_progress = false;
static uint8_t g_node_id = 0;
static volatile uint16_t g_last_nsub = 0;
static volatile uint16_t g_max_nsub = 0;
static volatile uint32_t g_csi_count = 0;

static void IRAM_ATTR csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf || info->len < 4 || g_udp_sock < 0 || g_ota_in_progress) return;
    uint16_t n = info->len / 2;
    g_last_nsub = n; if (n > g_max_nsub) g_max_nsub = n; g_csi_count++;
    size_t total = sizeof(wraith_pkt_t) + info->len;
    if (total > 8000) return;
    uint8_t buf[sizeof(wraith_pkt_t) + 4096];
    wraith_pkt_t *pkt = (wraith_pkt_t *)buf;
    pkt->magic=WRAITH_MAGIC; pkt->node_id=g_node_id; pkt->channel=info->rx_ctrl.channel;
    pkt->rssi=info->rx_ctrl.rssi; pkt->noise_floor=info->rx_ctrl.noise_floor;
    pkt->timestamp_us=(uint32_t)(esp_timer_get_time()&0xFFFFFFFF); pkt->n_sub=n;
    memcpy(buf+sizeof(wraith_pkt_t), info->buf, info->len);
    sendto(g_udp_sock, buf, total, MSG_DONTWAIT, (struct sockaddr*)&g_server, sizeof(g_server));
}

// === LINKER WRAP: Override get_channel_max_bandwidth ===
extern uint8_t __real_get_channel_max_bandwidth(uint8_t channel, void *ht_info);
uint8_t __wrap_get_channel_max_bandwidth(uint8_t channel, void *ht_info)
{
    // Force BW40 for all 5 GHz channels
    if (channel >= 36) {
        ESP_LOGW(TAG, "WRAP: get_channel_max_bandwidth(ch=%d) → BW40", channel);
        return 1; // BW40
    }
    return __real_get_channel_max_bandwidth(channel, ht_info);
}

// === LINKER WRAP: Override hal_mac_set_csi_cbw ===
extern void __real_hal_mac_set_csi_cbw(uint8_t cbw);
void __wrap_hal_mac_set_csi_cbw(uint8_t cbw)
{
    ESP_LOGW(TAG, "WRAP: hal_mac_set_csi_cbw(%d) — FORCING HE20", cbw);
    REG32(0x600A409C) = 0x0A;
}

// Standard boilerplate (same as before)
static void do_ota(void){g_ota_in_progress=true;char u[128];snprintf(u,128,"http://%s:%d/wraith_node.bin",SERVER_IP,OTA_PORT);esp_http_client_config_t h={.url=u,.timeout_ms=30000};esp_https_ota_config_t o={.http_config=&h};if(esp_https_ota(&o)==ESP_OK){vTaskDelay(pdMS_TO_TICKS(500));esp_restart();}else g_ota_in_progress=false;}
static void ota_task(void*a){int s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons(OTA_PORT),.sin_addr.s_addr=INADDR_ANY};bind(s,(struct sockaddr*)&addr,sizeof(addr));uint32_t b;while(1){if(recv(s,&b,4,0)==4&&b==OTA_CMD_MAGIC)do_ota();}}
static void wifi_evt(void*a,esp_event_base_t b,int32_t id,void*d){if(b==WIFI_EVENT){if(id==WIFI_EVENT_STA_START)esp_wifi_connect();else if(id==WIFI_EVENT_STA_DISCONNECTED){xEventGroupClearBits(g_wifi_events,WIFI_CONNECTED_BIT);esp_wifi_connect();}}else if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){uint8_t lo=((uint8_t*)&((ip_event_got_ip_t*)d)->ip_info.ip.addr)[3];if(lo==144)g_node_id=1;else if(lo==229)g_node_id=2;else if(lo==95)g_node_id=3;else if(lo==143)g_node_id=4;else g_node_id=lo%MAX_NODES;ESP_LOGI(TAG,"Node %d",g_node_id);xEventGroupSetBits(g_wifi_events,WIFI_CONNECTED_BIT);}}
static void init_wifi(void){g_wifi_events=xEventGroupCreate();ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_sta();wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&c));esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_evt,NULL);esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_evt,NULL);wifi_config_t sc={0};memcpy(sc.sta.ssid,WIFI_SSID,strlen(WIFI_SSID));memcpy(sc.sta.password,WIFI_PASS,strlen(WIFI_PASS));sc.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK;ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&sc));esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);wifi_bandwidths_t bw={.ghz_2g=WIFI_BW20,.ghz_5g=WIFI_BW40};esp_wifi_set_bandwidths(WIFI_IF_STA,&bw);ESP_ERROR_CHECK(esp_wifi_start());xEventGroupWaitBits(g_wifi_events,WIFI_CONNECTED_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
    wifi_bandwidth_t nbw; esp_wifi_get_bandwidth(WIFI_IF_STA,&nbw);
    ESP_LOGW(TAG,"Negotiated BW: %d",nbw);
    wifi_csi_config_t cc={.enable=1,.acquire_csi_legacy=1,.acquire_csi_ht20=1,.acquire_csi_ht40=1,.acquire_csi_su=1,.acquire_csi_mu=1};ESP_ERROR_CHECK(esp_wifi_set_csi_config(&cc));ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb,NULL));ESP_ERROR_CHECK(esp_wifi_set_csi(true));
    REG32(0x600A409C) = 0x0A;
    ESP_LOGW(TAG,"HE20+BW40 patch applied");}
static void init_udp(void){g_udp_sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);memset(&g_server,0,sizeof(g_server));g_server.sin_family=AF_INET;g_server.sin_port=htons(SERVER_PORT);inet_aton(SERVER_IP,&g_server.sin_addr);g_ping_target.sin_family=AF_INET;g_ping_target.sin_port=htons(12345);}
static void ping_task(void*a){const uint8_t p[]={'W','R','T','H'};esp_netif_t*n=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t ip;esp_netif_get_ip_info(n,&ip);g_ping_target.sin_addr.s_addr=ip.gw.addr;g_ping_sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);while(1){if(!g_ota_in_progress)sendto(g_ping_sock,p,4,MSG_DONTWAIT,(struct sockaddr*)&g_ping_target,sizeof(g_ping_target));vTaskDelay(pdMS_TO_TICKS(CSI_PING_INTERVAL));}}

// === WRAP ieee80211_set_phy_bw to force BW40 ===
extern void __real_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi);
void __wrap_ieee80211_set_phy_bw(uint8_t iface, uint8_t bw, uint8_t sgi)
{
    ESP_LOGW(TAG, "WRAP: ieee80211_set_phy_bw(iface=%d, bw=%d, sgi=%d) → forcing bw=2(BW40)", iface, bw, sgi);
    __real_ieee80211_set_phy_bw(iface, 2, sgi); /* 2 = BW40 */
}

void app_main(void){
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG,"=== WRAITH v%d — CBW40 PATCH ===",FW_VERSION);
    init_wifi(); init_udp();
    xTaskCreatePinnedToCore(ota_task,"ota",8192,NULL,3,NULL,0);
    xTaskCreatePinnedToCore(ping_task,"ping",2048,NULL,5,NULL,0);
    // Quick CSI mode scan with HT40 PHY
    ESP_LOGW(TAG, "==== CSI MODE SCAN WITH HT40 PHY ====");
    for (uint32_t v = 0; v <= 0x3E; v += 2) {
        g_csi_count=0; g_last_nsub=0; g_max_nsub=0;
        REG32(0x600A409C) = v;
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGW(TAG, "9C=0x%02lX → nsub=%d max=%d cnt=%lu", v, g_last_nsub, g_max_nsub, g_csi_count);
    }
    REG32(0x600A409C) = 0x0A;
    ESP_LOGW(TAG, "==== SCAN DONE ====");

    while(1){vTaskDelay(pdMS_TO_TICKS(5000));ESP_LOGI(TAG,"nsub=%d max=%d cnt=%lu",g_last_nsub,g_max_nsub,g_csi_count);}
}
