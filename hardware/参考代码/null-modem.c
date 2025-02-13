// reference "https://github.com/espressif/arduino-esp32/issues/10854"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include "driver/uart.h"
#include "esp_netif.h"

// UART configuration
#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_RX_PIN        10
#define UART_TX_PIN        11
#define UART_BUF_SIZE      1024

// PPP configuration
static ppp_pcb *ppp;
static struct netif ppp_netif;

// Function to handle PPP link status changes
static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx) {
    struct netif *pppif = (struct netif *)ctx;

    if (err_code == PPPERR_NONE) {
        printf("PPP connection established\n");
        printf("IP address: %s\n", ip4addr_ntoa(netif_ip4_addr(pppif)));
        printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(pppif)));
        printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(pppif)));
        return;
    }

    printf("PPP connection lost: %s\n");
}

// Function to output PPP data over UART
static u32_t pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
    uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    return len;
}

// Initialize UART
void uart_init() {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

// Main application
void setup() {
    // Initialize TCP/IP stack
    esp_netif_init();

    // Initialize UART
    uart_init();

    // Create PPPoS interface
    ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, &ppp_netif);
    if (ppp == NULL) {
        printf("Failed to create PPPoS interface\n");
        return;
    }

    // Set PPP as the default interface
    pppapi_set_default(ppp);

    // Connect PPP
    pppapi_connect(ppp, 0);
}

void loop() {
    uint8_t data[UART_BUF_SIZE];
    int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
    if (len > 0) {
        pppos_input(ppp, data, len);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
}