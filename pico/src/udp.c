#include "udp.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"

#define UDP_PORT 8080

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define ESP32_IP "192.168.4.1" // default

void udp_receiver_task(void *pvParameters) {
    printf("UDP receiver task started\n");

    int sock = *(int *)pvParameters;

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char buffer[256];

    int len;
    while (true) {
        len = lwip_recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr *)&sender_addr, &sender_len);

        if (len > 0) {
            buffer[len] = '\0';
            char sender_ip[16];
            strcpy(sender_ip, inet_ntoa(sender_addr.sin_addr));
            printf("UDP received from %s: %s\n", sender_ip, buffer);
        } else if (len < 0) {
            printf("UDP recvfrom error: %d\n", len);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    lwip_close(sock);
}

void udp_sender_task(void *pvParameters) {
    printf("UDP sender task started\n");

    vTaskDelay(pdMS_TO_TICKS(3000));

    int sock = *(int *)pvParameters;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton(ESP32_IP, &server_addr.sin_addr);

    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "UDP Hello from Pico #%d", msg_count++);

        int sent =
            lwip_sendto(sock, msg, strlen(msg), 0,
                        (struct sockaddr *)&server_addr, sizeof(server_addr));

        if (sent > 0) {
            printf("UDP sent: %s\n", msg);
        } else {
            printf("UDP send failed: %d\n", sent);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    lwip_close(sock);
}

void udp_task(void *pvParameters) {
    printf("UDP task started\n");

    vTaskDelay(pdMS_TO_TICKS(3000));

    int sock;
    struct sockaddr_in local_addr, server_addr, sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char buffer[256];

    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("failed to create UDP socket\n");
        vTaskDelete(NULL);
        return;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(UDP_PORT);

    if (lwip_bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) <
        0) {
        printf("failed to bind UDP socket\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return;
    }

    printf("Pico bound to port 8080\n");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton(ESP32_IP, &server_addr.sin_addr);

    xTaskCreate(udp_sender_task, "udp_tx", 1024, &sock, WORKER_TASK_PRIORITY,
                NULL);
    xTaskCreate(udp_receiver_task, "udp_rx", 1024, &sock, WORKER_TASK_PRIORITY,
                NULL);

    vTaskDelete(NULL);
}
