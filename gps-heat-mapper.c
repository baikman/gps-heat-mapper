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

// I2C defines for OLED display
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// UART defines and pins for GPS module
#define UART_ID uart1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BAUD_RATE 9600 // Note: Default baudrate for NEO6MV2 is 9600, but this can be increased.

bool led_state = false;

char buf[3];
char data[MINMEA_MAX_SENTENCE_LENGTH];
char line[MINMEA_MAX_SENTENCE_LENGTH];

int coor_ind = 0;

char lat[10000][15];
char lon[10000][15];
char time[10000][6];

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

static const char body[] =
    "<!DOCTYPE html><html><head><title>Pico 2W</title></head>"
    "<body><h1>Pico 2W Access Point</h1>"
    "<form action=\"/toggle\" method=\"get\">"
    "<button type=\"submit\">Toggle LED</button>"
    "</form></body></html>";

static char html_page[512]; // large enough buffer

void build_http_page(char *msg) {
    snprintf(html_page, sizeof(html_page),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (int)strlen(msg), msg);
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

bool parse_gga(const char *sentence, char *type) {
    
    int ind = 3;
    for (ind; ind <= 5; ind++) type[ind - 3] = sentence[ind];
    if (type[0] == 'G' && type[1] == 'G' && type[2] == 'A') {
        printf("NMEA: %s\n", sentence);

        ind++;
        int dind = 0;
        for (int i = 0; i < MINMEA_MAX_SENTENCE_LENGTH; i++) data[i] = '\0';

        while (sentence[ind] != ',') data[dind++] = sentence[ind++];

        ind++;
        dind = 0;
        for (int i = 0; i < MINMEA_MAX_SENTENCE_LENGTH; i++) data[i] = '\0';

        if (sentence[ind] == ',') {
            printf("No data\n");
            return false;
        }

        while (sentence[ind] != ',') {
            lat[coor_ind][dind] = sentence[ind];
            data[dind++] = sentence[ind++];
        }


        ind++;
        while (sentence[ind] != ',') {
            lat[coor_ind][dind] = sentence[ind];
            data[dind++] = sentence[ind++];
        }
        printf("Latitude: %s\n", data);
        build_http_page(data);

        ind++;
        dind = 0;
        for (int i = 0; i < MINMEA_MAX_SENTENCE_LENGTH; i++) data[i] = '\0';

        while (sentence[ind] != ',') {
            lon[coor_ind][dind] = sentence[ind];
            data[dind++] = sentence[ind++];
        }

        ind++;
        while (sentence[ind] != ',') {
            lon[coor_ind][dind] = sentence[ind];
            data[dind++] = sentence[ind++];
        }
        printf("Longitude: %s\n", data);

        coor_ind++;

        return true;
    }

    else if (type[0] == 'G' && type[1] == 'S' && type[2] == 'V') printf("NMEA: %s\n", sentence);
    return false;
}

void main(){
    stdio_init_all();

    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    if (cyw43_arch_init()) {
        blink_once(100);
        return;
    }

    // Enable AP mode
    cyw43_arch_enable_ap_mode("GPS-Heatmapper", "12345678", CYW43_AUTH_WPA2_AES_PSK);

    // Set Pico’s AP interface address
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192,168,4,1);
    IP4_ADDR(&netmask, 255,255,255,0);
    IP4_ADDR(&gw, 192,168,4,1);
    netif_set_addr(&cyw43_state.netif[CYW43_ITF_AP], &ipaddr, &netmask, &gw);

    build_http_page(body);
    
    // DHCP Initialization
    static dhcp_server_t dhcp;
    dhcp_server_init(&dhcp, &ipaddr, &netmask);

    // Setup TCP server on port 80
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, http_accept);
    
    int idx = 0;

    while (true) {
        cyw43_arch_poll(); // keep Wi-Fi + lwIP alive
        sys_check_timeouts();

        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n') {
                line[idx] = '\0';
                idx = 0;
                
                parse_gga(line, buf);

            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
}