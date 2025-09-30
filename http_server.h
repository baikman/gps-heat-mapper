#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"

// External variables that the HTTP server needs access to
extern bool led_state;
extern char html_page[512];

// Function declarations
void build_http_page(const char *msg);
err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);          
err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); 
err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err);      