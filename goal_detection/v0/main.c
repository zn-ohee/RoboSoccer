#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "lcd.h"
#include "adc.h"

// Laser sensor characteristics:
// Laser ON (beam hitting sensor): raw ~= 0
// Laser BLOCKED / NO LIGHT (Goal): raw > 1000
#define THRESHOLD 400

// If the raw ADC value jumps by more than this from the last displayed
// value, refresh the LCD immediately instead of waiting for the periodic update.
#define RAW_CHANGE_THRESHOLD 50

int main(void)
{
    // Configure PB0, PB1, PB2 as input with internal pull-up resistors (Active Low)
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2);

    lcd_init();
    adc_init();

    uint16_t score_a = 0;
    uint16_t score_b = 0; // fixed value for now

    uint8_t pb0_prev = 1;
    uint8_t pb1_prev = 1;
    uint8_t pb2_prev = 1;

    uint8_t lcd_timer = 0;
    uint16_t raw_adc_prev = 0;
    char buf[17];

    while (1)
    {
        // 1. Read ADC raw value on ADC0 (PA0), averaged to smooth out ambient light flicker
        uint16_t raw_adc = adc_read_avg(0, 10);

        // 2. Goal detection: when laser is BLOCKED (raw rises above THRESHOLD)
        if (raw_adc > THRESHOLD)
        {
            score_a++;
        }

        // 3. Read Active-Low Buttons with edge detection
        uint8_t pb0_now = (PINB & (1 << PB0)) ? 1 : 0;
        uint8_t pb1_now = (PINB & (1 << PB1)) ? 1 : 0;
        uint8_t pb2_now = (PINB & (1 << PB2)) ? 1 : 0;

        // Button 0 (PB0): Increment
        if (pb0_prev == 1 && pb0_now == 0)
        {
            score_a++;
        }

        // Button 1 (PB1): Decrement
        if (pb1_prev == 1 && pb1_now == 0)
        {
            if (score_a > 0)
            {
                score_a--;
            }
        }

        // Button 2 (PB2): Reset
        if (pb2_prev == 1 && pb2_now == 0)
        {
            score_a = 0;
        }

        pb0_prev = pb0_now;
        pb1_prev = pb1_now;
        pb2_prev = pb2_now;

        // 4. Update goal count on LCD every ~100 ms
        if (++lcd_timer >= 100)
        {
            lcd_timer = 0;

            snprintf(buf, sizeof(buf), "A %u- %u B       ", score_a, score_b);
            lcd_set_cursor(0, 0);
            lcd_print(buf);

            snprintf(buf, sizeof(buf), "Raw: %4u       ", raw_adc);
            lcd_set_cursor(1, 0);
            lcd_print(buf);

            raw_adc_prev = raw_adc;
        }
        // 5. Otherwise, update the raw value on the LCD as soon as it
        // fluctuates by more than RAW_CHANGE_THRESHOLD from the last shown value
        else if ((raw_adc > raw_adc_prev ? raw_adc - raw_adc_prev : raw_adc_prev - raw_adc) > RAW_CHANGE_THRESHOLD)
        {
            snprintf(buf, sizeof(buf), "Raw: %4u       ", raw_adc);
            lcd_set_cursor(1, 0);
            lcd_print(buf);

            raw_adc_prev = raw_adc;
        }

        _delay_ms(100);
    }
}