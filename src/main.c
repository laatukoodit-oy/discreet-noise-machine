#include <avr/pgmspace.h>
#include <util/delay.h>
#include "uart.h"

int main(void) {
    uart_init();

    while (1) {
        uart_write_P(PSTR("hello world\r\n"));
        _delay_ms(1000);
    }

    return 0;
}
