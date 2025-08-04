#include "tcp.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"

#define TCP_PORT 8081
#define ESP32_IP "192.168.4.1" // default

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void tcp_receiver_task(void *pvParameters) {
    printf("TCP receiver task started\n");

    tcp_context_t *ctx = (tcp_context_t *)pvParameters;
    int sock = ctx->sock;
    char buffer[256];
    int len;

    while (true) {
        len = lwip_recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len > 0) {
            buffer[len] = '\0';
            printf("TCP received: %s\n", buffer);
        } else if (len == 0) {
            printf("TCP connection closed by server\n");
            break;
        } else {
            printf("TCP recv error: %d\n", len);
            break;
        }
    }

    lwip_close(sock);
    free(ctx);
    vTaskDelete(NULL);
}

void tcp_sender_task(void *pvParameters) {
    printf("TCP sender task started\n");

    tcp_context_t *ctx = (tcp_context_t *)pvParameters;
    int sock = ctx->sock;

    vTaskDelay(pdMS_TO_TICKS(1000));

    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "TCP Hello from Pico #%d", msg_count++);

        if (lwip_send(sock, msg, strlen(msg), 0) < 0) {
            printf("TCP send failed\n");
            break;
        }

        printf("TCP sent: %s\n", msg);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    free(ctx);
    vTaskDelete(NULL);
}

void tcp_task(void *pvParameters) {
    printf("TCP task started\n");

    vTaskDelay(pdMS_TO_TICKS(3000));

    while (true) {
        int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            printf("failed to create TCP socket\n");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TCP_PORT);
        inet_aton(ESP32_IP, &server_addr.sin_addr);

        printf("attempting TCP connection to ESP32...\n");
        if (lwip_connect(sock, (struct sockaddr *)&server_addr,
                         sizeof(server_addr)) < 0) {
            printf("TCP connection failed\n");
            lwip_close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        printf("TCP connected to ESP32\n");

        tcp_context_t *rx_ctx = (tcp_context_t *)malloc(sizeof(tcp_context_t));
        tcp_context_t *tx_ctx = (tcp_context_t *)malloc(sizeof(tcp_context_t));

        if (!rx_ctx || !tx_ctx) {
            printf("failed to allocate TCP context\n");
            if (rx_ctx)
                free(rx_ctx);
            if (tx_ctx)
                free(tx_ctx);
            lwip_close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        rx_ctx->sock = sock;
        tx_ctx->sock = sock;

        xTaskCreate(tcp_receiver_task, "tcp_rx", 1024, rx_ctx,
                    WORKER_TASK_PRIORITY, NULL);
        xTaskCreate(tcp_sender_task, "tcp_tx", 1024, tx_ctx,
                    WORKER_TASK_PRIORITY, NULL);

        vTaskDelay(
            pdMS_TO_TICKS(10000));
    }

    vTaskDelete(NULL);
}
