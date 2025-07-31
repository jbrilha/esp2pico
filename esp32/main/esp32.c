#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"

#define WIFI_SSID "aa"
#define WIFI_PASS "bb"
#define PICO_IP "cc"
#define PICO_PORT 8080

static const char *TAG = "UDP_SENDER";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi connected!");
    }
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void udp_sender_task(void *pvParameters)
{
    int sock;
    struct sockaddr_in dest_addr;
    
    // wait for Wi-Fi connection
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    dest_addr.sin_addr.s_addr = inet_addr(PICO_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PICO_PORT);

    int msg_count = 0;
    while (1) {
        char message[64];
        snprintf(message, sizeof(message), "Hello from ESP32 #%d", msg_count++);
        
        int err = sendto(sock, message, strlen(message), 0, 
                        (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "error sending: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "sent: %s", message);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    
    wifi_init();
    
    xTaskCreate(udp_sender_task, "udp_sender_task", 4096, NULL, 5, NULL);
}
