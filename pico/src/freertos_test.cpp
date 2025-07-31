#include "pico_scroll.hpp" // cpp headers first!!

#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/pbuf.h"
#include "lwip/sockets.h"
#include "lwip/udp.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define WIFI_SSID "aa"
#define WIFI_PASSWORD "bb"
#define UDP_PORT 8080

// Which core to run on if configNUMBER_OF_CORES==1
#ifndef RUN_FREE_RTOS_ON_CORE
#define RUN_FREE_RTOS_ON_CORE 0
#endif

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

using namespace pimoroni;
PicoScroll pico_scroll;

void udp_receiver_task(void *pvParameters) {
    printf("UDP receiver task started\n");
    
    int sock;
    struct sockaddr_in local_addr, sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char buffer[256];
    
    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("failed to create socket\n");
        vTaskDelete(NULL);
        return;
    }
    
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(8080);
    
    if (lwip_bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        printf("failed to bind socket\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    while (true) {
        int bytes = lwip_recvfrom(sock, buffer, sizeof(buffer)-1, 0,
                           (struct sockaddr*)&sender_addr, &sender_len);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            char sender_ip[16];
            strcpy(sender_ip, inet_ntoa(sender_addr.sin_addr));
            printf("received from %s: %s\n", sender_ip, buffer);
        } else if (bytes < 0) {
            printf("recvfrom error: %d\n", bytes);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    lwip_close(sock);
}

void wifi_connect_task(void *pvParameters) {
    printf("Wi-Fi task started\n");
    
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        vTaskDelete(NULL);
        return;
    }
    
    cyw43_arch_enable_sta_mode();
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, 
                                          CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect to Wi-Fi\n");
        vTaskDelete(NULL);
        return;
    }
    
    printf("connected to Wi-Fi!!!\n");
    printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    
    xTaskCreate(udp_receiver_task, "udp_task", 2048, NULL, WORKER_TASK_PRIORITY, NULL);
    
    // this task is done, can kill itself 
    vTaskDelete(NULL);
}

void scroll_task(__unused void *params) {
    pico_scroll.init();

    while (true) {
        printf("123\n");
        pico_scroll.scroll_text("123", 255, 100);
        sleep_ms(500);
        pico_scroll.clear();
        sleep_ms(500);
        printf("456\n");
        pico_scroll.scroll_text("456", 255, 100);
        pico_scroll.clear();
        sleep_ms(500);
    }
}

void main_task(__unused void *params) {

    xTaskCreate(scroll_task, "scroll_task", SCROLL_TASK_STACK_SIZE, NULL,
                SCROLL_TASK_PRIORITY, NULL);

    xTaskCreate(wifi_connect_task, "wifi_task", 2048, NULL, WORKER_TASK_PRIORITY, NULL);


    int count = 0;
    while (true) {
#if configNUMBER_OF_CORES > 1
        static int last_core_id = -1;
        if (portGET_CORE_ID() != last_core_id) {
            last_core_id = portGET_CORE_ID();
            printf("main task is on core %d\n", last_core_id);
        }
#endif
        printf("Hello from main task count=%u\n", count++);
        vTaskDelay(3000);
    }
}

void vLaunch(void) {
    TaskHandle_t task;
    xTaskCreate(main_task, "main_thread", MAIN_TASK_STACK_SIZE, NULL,
                MAIN_TASK_PRIORITY, &task);

#if configUSE_CORE_AFFINITY && configNUMBER_OF_CORES > 1
    // we must bind the main task to one core (well at least while the init is
    // called)
    vTaskCoreAffinitySet(task, 1);
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main(void) {
    stdio_init_all();

    const char *rtos_name;
#if (configNUMBER_OF_CORES > 1)
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if (configNUMBER_OF_CORES > 1)
    printf("Starting %s on both cores:\n", rtos_name);
    vLaunch();
#elif (RUN_FREE_RTOS_ON_CORE == 1 && configNUMBER_OF_CORES == 1)
    printf("Starting %s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true)
        ;
#else
    printf("Starting %s on core 0:\n", rtos_name);
    vLaunch();
#endif
    return 0;
}
