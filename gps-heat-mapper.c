#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "minmea.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/timeouts.h"
#include "dhcpserver.h"

// I2C defines
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// UART defines
#define UART_ID uart1
#define BAUD_RATE 115200

// Pins for GPS module
#define UART_TX_PIN 4
#define UART_RX_PIN 5

bool led_state = false;

char line[MINMEA_MAX_SENTENCE_LENGTH];

typedef struct {
    uint32_t loc_id;
    uint16_t count;
} Heatmap;

void blink_once(int ms) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    sleep_ms(ms);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    sleep_ms(ms);
}

void blink_times(int times, int ms) {
    for (int i = 0; i < times; i++) {
        blink_once(ms);
    }
}

static const char body[] =
    "<!DOCTYPE html><html><head><title>Pico 2W</title></head>"
    "<body><h1>Pico 2W Access Point</h1>"
    "<form action=\"/toggle\" method=\"get\">"
    "<button type=\"submit\">Toggle LED</button>"
    "</form></body></html>";

static char html_page[512]; // large enough buffer

void build_http_page() {
    snprintf(html_page, sizeof(html_page),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (int)strlen(body), body);
}

// Callback function for ack and closes connection
// https://www.nongnu.org/lwip/2_1_x/group__tcp__raw.html#ga1596332b93bb6249179f3b89f24bd808
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    tcp_close(tpcb);
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Copy request data
    char *data = malloc(p->len + 1);

    if (!data) {
        pbuf_free(p);
        return ERR_MEM;
    }

    memcpy(data, p->payload, p->len);
    data[p->len] = '\0';

    // Handle toggle
    if (strstr(data, "GET /toggle") != NULL) {
        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
    }

    free(data);
    pbuf_free(p);

    // Send response
    // *** Page did not load on my iPhone (safari) when using sizeof(html_page) - 1, but using strlen(html_page) works
    // sizeof(html_page) - 1 did work on windows laptop (chrome)
    tcp_write(tpcb, html_page, strlen(html_page), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Close connection after sending
    tcp_sent(tpcb, http_sent);

    return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    tcp_arg(newpcb, NULL);
    return ERR_OK;
}


void main(){
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("CYW43 init failed\n");
        return;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // ensure off

    // access point or smth
    cyw43_arch_enable_ap_mode("Pico2W-AP", "12345678", CYW43_AUTH_WPA2_AES_PSK);

    // Set Pico’s AP interface address
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192,168,4,1);
    IP4_ADDR(&netmask, 255,255,255,0);
    IP4_ADDR(&gw, 192,168,4,1);
    netif_set_addr(&cyw43_state.netif[CYW43_ITF_AP], &ipaddr, &netmask, &gw);

    build_http_page();
    
    // dhcp init type beat
    static dhcp_server_t dhcp;
    dhcp_server_init(&dhcp, &ipaddr, &netmask);

    // Setup TCP server on port 80
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, http_accept);

    while (true) {
        cyw43_arch_poll(); // keep Wi-Fi + lwIP alive
        sys_check_timeouts();
    }


    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    uart_init(UART_ID, 9600);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    int idx = 0;

    while (true) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n') {
                line[idx] = '\0';
                idx = 0;
                
                printf("NMEA: %s\n", line);
                if (minmea_sentence_id(line, false) == MINMEA_SENTENCE_RMC) {
                    struct minmea_sentence_rmc frame;
                    if (minmea_parse_rmc(&frame, line)) {
                        printf("Lat: %f, Lon: %f\n", minmea_tocoord(&frame.latitude), minmea_tocoord(&frame.longitude));
                    }
                }

            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
}