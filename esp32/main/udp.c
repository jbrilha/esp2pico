#include "udp.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#define MAX_MSG_SIZE 128
#define MAX_PEERS 5

#define UDP_PORT 8080

#define UDP_RECEIVER_TASK_STACK_SIZE 4096
#define UDP_SENDER_TASK_STACK_SIZE 2048
#define UDP_RECEIVER_TASK_PRIORITY 4
#define UDP_SENDER_TASK_PRIORITY 3

static const char *TAG = "ESP32";

typedef struct {
    struct sockaddr_in addr;
    uint32_t last_seen;
} peer_t;

typedef struct {
    peer_t peers[MAX_PEERS];
    uint8_t count;
    SemaphoreHandle_t mutex;
} active_peers_t;

active_peers_t active_peers;

void udp_sender_task(void *pvParameters) {
    int sock = *(int *)pvParameters;

    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "UDP Hello from ESP32 #%d", msg_count++);

        peer_t peers_copy[MAX_PEERS];
        int peer_count = 0;

        if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(50))) {
            peer_count = active_peers.count;
            memcpy(peers_copy, active_peers.peers, sizeof(peer_t) * peer_count);
            xSemaphoreGive(active_peers.mutex);
        }

        // send without holding mutex
        for (int i = 0; i < peer_count; i++) {
            if (sendto(sock, msg, strlen(msg), 0,
                       (struct sockaddr *)&peers_copy[i].addr,
                       sizeof(peers_copy[i].addr)) > 0) {
                ESP_LOGI(TAG, "UDP sent: %s", msg);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// this one should only (!!) be called in betwen taking and giving the mutex
static bool peer_exists_unsafe(const struct sockaddr_in *peer_addr) {
    peer_t peer;
    for (int i = 0; i < active_peers.count; i++) {
        peer = active_peers.peers[i];

        if (peer.addr.sin_addr.s_addr == peer_addr->sin_addr.s_addr &&
            peer.addr.sin_port == peer_addr->sin_port) {
            return true;
        }
    }

    return false;
}

bool peer_exists(const struct sockaddr_in *peer_addr) {
    bool exists = false;

    if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(100))) { // 100 ms
        exists = peer_exists_unsafe(peer_addr);
        xSemaphoreGive(active_peers.mutex);
    }

    return exists;
}

bool add_peer(const struct sockaddr_in *peer_addr) {
    bool res = false;

    if (xSemaphoreTake(active_peers.mutex, pdMS_TO_TICKS(100))) { // 100 ms
        if (peer_exists_unsafe(peer_addr)) {
            res = true;
        } else if (active_peers.count < MAX_PEERS) {

            active_peers.peers[active_peers.count] = (peer_t){
                .addr = *peer_addr,
                .last_seen = xTaskGetTickCount(),
            };

            active_peers.count++;
            res = true;
        }

        xSemaphoreGive(active_peers.mutex);
    }

    return res;
}

void udp_receiver_task(void *pvParameters) {
    int sock = *(int *)pvParameters;

    struct sockaddr_in src_in;
    struct sockaddr *src = (struct sockaddr *)&src_in;
    socklen_t slen = sizeof(src_in);

    char rx_buf[MAX_MSG_SIZE];

    int len;
    while (true) {
        len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0, src, &slen);

        if (len > 0) {
            rx_buf[len] = '\0';
            char peer_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_in.sin_addr, peer_ip, INET_ADDRSTRLEN);

            ESP_LOGI(TAG, "UDP RX [%s:%d]: %s", peer_ip, ntohs(src_in.sin_port),
                     rx_buf);

            if (add_peer(&src_in)) {
                ESP_LOGI(TAG, "added peer");
            } else {
                ESP_LOGW(TAG, "failed to add peer");
            }
        }
    }
}

void udp_server_task(void *pvParameters) {
    int sock;
    struct sockaddr_in bind_addr;
    socklen_t slen = sizeof(bind_addr);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "failed to create UDP socket");
        vTaskDelete(NULL);
        return;
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(UDP_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&bind_addr, slen) < 0) {
        ESP_LOGE(TAG, "failed to bind UDP socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP server bound to port 8080");

    active_peers.count = 0;
    memset(active_peers.peers, 0, sizeof(active_peers.peers));
    active_peers.mutex = xSemaphoreCreateMutex();
    if (active_peers.mutex == NULL) {
        ESP_LOGE(TAG, "failed to create peers mutex");
        return;
    }


    xTaskCreate(udp_receiver_task, "udp_rx", UDP_RECEIVER_TASK_STACK_SIZE,
                &sock, UDP_RECEIVER_TASK_PRIORITY, NULL);
    xTaskCreate(udp_sender_task, "udp_tx", UDP_SENDER_TASK_STACK_SIZE, &sock,
                UDP_SENDER_TASK_PRIORITY, NULL);

    vTaskDelete(NULL);
}

