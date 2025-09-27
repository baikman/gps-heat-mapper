#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "minmea.h"

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

#include "pico/stdlib.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif
#define LED_PIN CYW43_WL_GPIO_LED_PIN

char buf[3];
char data[MINMEA_MAX_SENTENCE_LENGTH];
char line[MINMEA_MAX_SENTENCE_LENGTH];

int coor_ind = 0;

char lat[10000][15];
char lon[10000][15];

typedef struct {
    uint32_t loc_id;
    uint16_t count;
} Heatmap;

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

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    cyw43_arch_init();


    for (int i = 0; i < 6; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
        sleep_ms(250);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
        sleep_ms(250);
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
                
                // printf("NMEA: %s\n", line);
                parse_gga(line, buf);
                // if (minmea_sentence_id(line, false) == MINMEA_SENTENCE_RMC) {
                //     printf("Found RMC\n");
                //     struct minmea_sentence_rmc frame;
                //     if (minmea_parse_rmc(&frame, line)) {
                //         printf("Lat: %f, Lon: %f\n", minmea_tocoord(&frame.latitude), minmea_tocoord(&frame.longitude));
                //     }
                // }

            } else if (idx < sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
}