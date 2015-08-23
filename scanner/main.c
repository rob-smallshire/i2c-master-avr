#include <avr/io.h>
#include <avr/interrupt.h>

#include <elapsed.h>

long milliseconds_since;

void flash_led ()
{
    unsigned long milliseconds_current = millis();

    if (milliseconds_current - milliseconds_since > 1000) {
        // LED connected to PB7
        PORTB ^= (1 << PB7);
        milliseconds_since = milliseconds_current;
    }
}

int main(void)
{
    init_millis();

    // Now enable global interrupts
    sei();

    milliseconds_since = millis();

    // PC0/Analog 0 to Output
    DDRB |= (1 << PB7);
 
    while (1)
    {
        flash_led();
    }
}
