#include "tcp.h"
#include "esp_log.h"

static const char *TAG = "ESP32";

#define MAX_MSG_SIZE 128

#define TCP_PORT 8081

#define TCP_RECEIVER_TASK_STACK_SIZE 4096
#define TCP_SENDER_TASK_STACK_SIZE 2048
#define TCP_RECEIVER_TASK_PRIORITY 4
#define TCP_SENDER_TASK_PRIORITY 3

void tcp_receiver_task(void *pvParameters) {
    tcp_client_context_t *ctx = (tcp_client_context_t *)pvParameters;
    int client_sock = ctx->sock;
    char rx_buf[MAX_MSG_SIZE];
    int len;

    while (true) {
        len = recv(client_sock, rx_buf, sizeof(rx_buf) - 1, 0);
        if (len > 0) {
            rx_buf[len] = '\0';
            ESP_LOGI(TAG, "TCP RX: %s", rx_buf);
        } else if (len == 0) {
            ESP_LOGI(TAG, "TCP client disconnected");
            break;
        } else {
            ESP_LOGE(TAG, "TCP recv error: %d", len);
            break;
        }
    }

    close(client_sock); // Clean up socket
    free(ctx);          // Free allocated context
    vTaskDelete(NULL);
}

void tcp_sender_task(void *pvParameters) {
    tcp_client_context_t *ctx = (tcp_client_context_t *)pvParameters;
    int client_sock = ctx->sock;

    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "TCP Hello from ESP32 #%d", msg_count++);

        if (send(client_sock, msg, strlen(msg), 0) < 0) {
            ESP_LOGE(TAG, "TCP send failed");
            break;
        }

        ESP_LOGI(TAG, "TCP sent: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Don't close socket here - let receiver handle it
    free(ctx);
    vTaskDelete(NULL);
}

void tcp_server_task(void *pvParameters) {
    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "failed to create TCP socket");
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "failed to bind TCP socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(TAG, "failed to listen on TCP socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "TCP server listening on port 8081");

    while (true) {
        client_sock =
            accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            ESP_LOGE(TAG, "TCP accept failed");
            continue;
        }

        // Allocate separate context for each client
        tcp_client_context_t *rx_ctx = malloc(sizeof(tcp_client_context_t));
        tcp_client_context_t *tx_ctx = malloc(sizeof(tcp_client_context_t));

        if (!rx_ctx || !tx_ctx) {
            ESP_LOGE(TAG, "Failed to allocate client context");
            if (rx_ctx)
                free(rx_ctx);
            if (tx_ctx)
                free(tx_ctx);
            close(client_sock);
            continue;
        }

        rx_ctx->sock = client_sock;
        rx_ctx->client_addr = client_addr;
        tx_ctx->sock = client_sock;
        tx_ctx->client_addr = client_addr;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        ESP_LOGI(TAG, "TCP client connected: %s:%d", client_ip,
                 ntohs(client_addr.sin_port));

        xTaskCreate(tcp_receiver_task, "tcp_rx", TCP_RECEIVER_TASK_STACK_SIZE,
                    rx_ctx, TCP_RECEIVER_TASK_PRIORITY, NULL);
        xTaskCreate(tcp_sender_task, "tcp_tx", TCP_RECEIVER_TASK_STACK_SIZE,
                    tx_ctx, TCP_SENDER_TASK_PRIORITY, NULL);
    }
}
