#ifndef F_CPU
#define F_CPU 1000000UL // ATmega32 default fuses: internal RC osc, no divider
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "uart.h"

#define BAUD 9600
// Double Speed Mode (U2X = 1): F_CPU / (8 * BAUD) - 1
#define MYUBRR ((F_CPU / (8UL * BAUD)) - 1)

int main(void)
{
    uart_init(MYUBRR);

    while (1)
    {
        uart_transmit('1');
        _delay_ms(1000);

        uart_transmit('0');
        _delay_ms(1000);
    }
}
