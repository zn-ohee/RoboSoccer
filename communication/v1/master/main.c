#ifndef F_CPU
#define F_CPU 1000000UL // ATmega32 default fuses: internal RC osc, no divider
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "uart.h"

#define BAUD 9600
// Double Speed Mode (U2X = 1): F_CPU / (8 * BAUD) - 1
#define MYUBRR ((F_CPU / (8UL * BAUD)) - 1)

// Each word is sent as its characters followed by a newline, so the slave
// can tell where one word ends and the next begins.
static const char *words[] = { "2205031 - Arjo", "2205035 - Sanjoy", "2205059 - Ohee" };
#define NUM_WORDS (sizeof(words) / sizeof(words[0]))

static void uart_send_word(const char *word)
{
    while (*word)
        uart_transmit(*word++);
    uart_transmit('\n');
}

int main(void)
{
    uart_init(MYUBRR);

    uint8_t index = 0;

    while (1)
    {
        uart_send_word(words[index]);
        index = (index + 1) % NUM_WORDS;

        _delay_ms(2000);
    }
}
