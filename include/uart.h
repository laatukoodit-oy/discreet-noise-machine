#pragma once

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/io.h>

#define UART_WRITE_PSTR(s) uart_write_P(PSTR(s))
#define SET_UART_PIN DDRB |= _BV(PB1)
#define SET_UART_INACTIVE PORTB |= _BV(PB1)


void uart_init();
void uart_putchar(char byte);
void uart_write(const char *data);
void uart_write_P(const char *data);