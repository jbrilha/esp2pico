#ifndef UDP_H
#define UDP_H

#include "FreeRTOS.h"
#include "task.h"

void udp_receiver_task(void *pvParameters);
void udp_sender_task(void *pvParameters);
void udp_task(void *pvParameters);

#endif // !UDP_H

