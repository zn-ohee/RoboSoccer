#ifndef F_CPU
#define F_CPU 1000000UL // ATmega32 default fuses: internal RC osc, no divider
#endif

#include <avr/io.h>
#include "uart.h"

#define BAUD 9600
// Double Speed Mode (U2X = 1): F_CPU / (8 * BAUD) - 1
#define MYUBRR ((F_CPU / (8UL * BAUD)) - 1)

int main(void)
{
    DDRB |= (1 << PB0); // PB0 as output for the LED

    uart_init(MYUBRR);

    while (1)
    {
        char c = uart_receive(); // blocks until a byte arrives

        if (c == '1')
            PORTB |= (1 << PB0);
        else if (c == '0')
            PORTB &= ~(1 << PB0);
    }
}
