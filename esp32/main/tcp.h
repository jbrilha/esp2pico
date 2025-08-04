#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

typedef struct {
    int sock;
    struct sockaddr_in client_addr;
} tcp_client_context_t;

void tcp_receiver_task(void *pvParameters);
void tcp_sender_task(void *pvParameters);
void tcp_server_task(void *pvParameters);

#endif // !TCP_SERVER_H

