#ifndef F_CPU
#define F_CPU 1000000UL // ATmega32 default fuses: internal RC osc, no divider
#endif

#include <avr/io.h>
#include "lcd.h"
#include "uart.h"

#define BAUD 9600
// Double Speed Mode (U2X = 1): F_CPU / (8 * BAUD) - 1
#define MYUBRR ((F_CPU / (8UL * BAUD)) - 1)

#define WORD_BUF_SIZE 17 // matches one 16-char LCD line + terminator

int main(void)
{
    uart_init(MYUBRR);
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("Received:");

    char word[WORD_BUF_SIZE];
    uint8_t pos = 0;

    while (1)
    {
        char c = uart_receive(); // blocks until a byte arrives

        if (c == '\n')
        {
            word[pos] = '\0';
            pos = 0;

            lcd_set_cursor(1, 0);
            lcd_print("                "); // clear the line
            lcd_set_cursor(1, 0);
            lcd_print(word);
        }
        else if (pos < WORD_BUF_SIZE - 1)
        {
            word[pos++] = c;
        }
    }
}
