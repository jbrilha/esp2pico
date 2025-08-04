#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void udp_receiver_task(void *pvParameters);
void udp_sender_task(void *pvParameters);
void udp_server_task(void *pvParameters);

#endif // !UDP_SERVER_H

