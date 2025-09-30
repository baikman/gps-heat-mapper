#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"

void build_http_page(const char *msg) {
    snprintf(html_page, 512,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (int)strlen(msg), msg);
}

// Callback function for ack and closes connection
err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) { 
    tcp_close(tpcb);
    return ERR_OK;
}

err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) { 
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
    tcp_write(tpcb, html_page, strlen(html_page), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Close connection after sending
    tcp_sent(tpcb, http_sent);

    return ERR_OK;
}

err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) { 
    tcp_recv(newpcb, http_recv);
    tcp_arg(newpcb, NULL);
    return ERR_OK;
}