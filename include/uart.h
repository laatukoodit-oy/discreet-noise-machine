#pragma once
#include <stdio.h>

extern FILE uart_output;

void uart_init(void);
int uart_putchar(char c, FILE *stream);
