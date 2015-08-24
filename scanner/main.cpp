#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

extern "C" {
#include <elapsed.h>
}

#include <i2c.h>

extern "C" {
#include "stream.h"
#include "uart.h"
}

int main (void)
{
    init_millis();

    sei();

    stdin = stdout = get_uart0_stream();

    // USB Serial 0
    uart0_init(UART_BAUD_SELECT(9600, F_CPU));

    printf("Initializing I2C\n");
    I2C::begin();

    printf("Scanning I2C\n");
    for (uint8_t address = 1; address <= 0x7f; ++address)
    {
        int result = I2C::scan(address);
	printf("%d %d\n", address, result);
    }
    printf("Done.\n");
    I2C::end();

    while (1) {
    };
}
