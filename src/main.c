#include <util/delay.h>
#include "uart.h"

int main(void) {
    uart_init();
    stdout = &uart_output; // Redirect stdout to UART

    while (1) {
        printf("hello world\r\n");
        _delay_ms(1000);
    }

    return 0;
}
