/* Counting Milliseconds with Timer1
 * ---------------------------------
 * For more information see
 * http://www.adnbr.co.uk/articles/counting-milliseconds
 *
 * 620 bytes - ATmega - 16MHz
 */

// 16MHz Clock
//#define F_CPU 16000000UL
 
// Calculate the value needed for 
// the CTC match value in OCR1A.
#define CTC_MATCH_OVERFLOW ((F_CPU / 1000) / 8) 
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
 
volatile unsigned long timer1_millis;
 
ISR (TIMER1_COMPA_vect)
{
    timer1_millis++;
}

unsigned long millis()
{
    unsigned long millis_return;

    // Ensure this cannot be disrupted
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        millis_return = timer1_millis;
    }
 
    return millis_return;
}
 
void init_millis(void)
{
    // CTC mode, Clock/8
    TCCR1B |= (1 << WGM12) | (1 << CS11);
 
    // Load the high byte, then the low byte
    // into the output compare
    OCR1AH = (CTC_MATCH_OVERFLOW >> 8);
    OCR1AL = (uint8_t)CTC_MATCH_OVERFLOW;
 
    // Enable the compare match interrupt
    TIMSK1 |= (1 << OCIE1A);
}


