#ifndef F_CPU
#define F_CPU 1000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include "lcd.h"
#include "adc.h"
#include "uart.h"

// Laser sensor characteristics:
// Laser ON (beam hitting sensor): raw ~= 0
// Laser BLOCKED / NO LIGHT (Goal): raw > 1000
#define THRESHOLD 400

// If the raw ADC value jumps by more than this from the last displayed
// value, refresh the LCD immediately instead of waiting for the periodic update.
#define RAW_CHANGE_THRESHOLD 50

#define BAUD 9600
// Double Speed Mode (U2X = 1): F_CPU / (8 * BAUD) - 1
#define MYUBRR ((F_CPU / (8UL * BAUD)) - 1)

// This board owns team A: its own laser/buttons drive score_a, which is
// broadcast to the other board over UART. score_b is whatever the other
// board (team B) last reported.
static uint16_t score_a = 0;
static uint16_t score_b = 0;

// Sends "S<count>\n" so the peer board updates its copy of *our* score.
static void uart_send_score(uint16_t score)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "S%u\n", score);
    for (char *p = buf; *p; p++)
        uart_transmit(*p);
}

// Feeds any bytes received so far into a small line buffer and, once a full
// "S<count>\n" line has arrived, updates the peer's score (team B for us).
static void uart_poll_rx(void)
{
    static char line[8];
    static uint8_t pos = 0;

    while (uart_available())
    {
        char c = uart_receive();

        if (c == '\n')
        {
            line[pos] = '\0';
            pos = 0;

            if (line[0] == 'S')
                score_b = (uint16_t)atoi(&line[1]);
        }
        else if (pos < sizeof(line) - 1)
        {
            line[pos++] = c;
        }
        else
        {
            pos = 0; // malformed/overlong line - resync
        }
    }
}

int main(void)
{
    // Configure PB0, PB1, PB2 as input with internal pull-up resistors (Active Low)
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2));
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2);

    lcd_init();
    adc_init();
    uart_init(MYUBRR);

    // The very first ADC conversion after adc_init() can return a bogus
    // value (ADC needs a warm-up read once the reference is selected), so
    // take one throwaway reading before deciding the laser's starting
    // state - otherwise a spurious high first sample looks like a
    // beam-block edge and counts a goal before the board has even settled.
    adc_read_avg(0, 10);
    uint16_t raw_adc_boot = adc_read_avg(0, 10);

    uint8_t pb0_prev = 1;
    uint8_t pb1_prev = 1;
    uint8_t pb2_prev = 1;

    uint8_t lcd_timer = 0;
    uint16_t heartbeat_timer = 0;
    uint16_t raw_adc_prev = raw_adc_boot;
    uint8_t laser_blocked_prev = (raw_adc_boot > THRESHOLD) ? 1 : 0;
    char buf[17];

    while (1)
    {
        uart_poll_rx();

        // 1. Read ADC raw value on ADC0 (PA0), averaged to smooth out ambient light flicker
        uint16_t raw_adc = adc_read_avg(0, 10);

        uint8_t score_changed = 0;

        // 2. Goal detection: count once on the transition into BLOCKED (raw
        // rises above THRESHOLD), not on every loop iteration the beam
        // stays blocked - otherwise the score keeps climbing the whole time
        // something is in the way (e.g. jumps by 10 each 100ms LCD refresh).
        uint8_t laser_blocked_now = (raw_adc > THRESHOLD) ? 1 : 0;
        if (laser_blocked_now && !laser_blocked_prev)
        {
            score_a++;
            score_changed = 1;
        }
        laser_blocked_prev = laser_blocked_now;

        // 3. Read Active-Low Buttons with edge detection
        uint8_t pb0_now = (PINB & (1 << PB0)) ? 1 : 0;
        uint8_t pb1_now = (PINB & (1 << PB1)) ? 1 : 0;
        uint8_t pb2_now = (PINB & (1 << PB2)) ? 1 : 0;

        // Button 0 (PB0): Increment
        if (pb0_prev == 1 && pb0_now == 0)
        {
            score_a++;
            score_changed = 1;
        }

        // Button 1 (PB1): Decrement
        if (pb1_prev == 1 && pb1_now == 0)
        {
            if (score_a > 0)
            {
                score_a--;
                score_changed = 1;
            }
        }

        // Button 2 (PB2): Reset
        if (pb2_prev == 1 && pb2_now == 0)
        {
            score_a = 0;
            score_changed = 1;
        }

        pb0_prev = pb0_now;
        pb1_prev = pb1_now;
        pb2_prev = pb2_now;

        // 4. Tell the other board about our new score as soon as it changes,
        // and also resend periodically (every ~300ms) so a board that reboots,
        // misses a byte, or was mid-reconnect over the HC-05 link (which can
        // itself take several seconds to resync) picks up the correct count
        // as soon as the radio link is back up, instead of waiting up to 2s.
        if (score_changed)
        {
            uart_send_score(score_a);
            heartbeat_timer = 0;
        }
        else if (++heartbeat_timer >= 30) // 30 * 10ms = 300ms
        {
            heartbeat_timer = 0;
            uart_send_score(score_a);
        }

        // 5. Update goal count on LCD every ~100 ms
        if (++lcd_timer >= 10)
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
        // 6. Otherwise, update the raw value on the LCD as soon as it
        // fluctuates by more than RAW_CHANGE_THRESHOLD from the last shown value
        else if ((raw_adc > raw_adc_prev ? raw_adc - raw_adc_prev : raw_adc_prev - raw_adc) > RAW_CHANGE_THRESHOLD)
        {
            snprintf(buf, sizeof(buf), "Raw: %4u       ", raw_adc);
            lcd_set_cursor(1, 0);
            lcd_print(buf);

            raw_adc_prev = raw_adc;
        }

        _delay_ms(10);
    }
}
