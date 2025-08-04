#ifndef TCP_H
#define TCP_H

#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    int sock;
} tcp_context_t;

void tcp_task(void *pvParameters);
void tcp_sender_task(void *pvParameters);
void tcp_server_task(void *pvParameters);

#endif // !TCP_H

