#pragma once

#define UART_WRITE_PSTR(s) uart_write_P(PSTR(s))

void uart_init();
void uart_putchar(char byte);
void uart_write(const char *data);
void uart_write_P(const char *data);

