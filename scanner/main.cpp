#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <i2c.h>

extern "C" {
#include "stream.h"
#include "uart.h"
}

int main (void)
{
    sei();

    stdin = stdout = get_uart0_stream();

    // USB Serial 0
    uart0_init(UART_BAUD_SELECT(9600, F_CPU));

    printf("Scanning I2C\n");
    for (uint8_t address = 1; address <= 0x7f; ++address)
    {
        int result = I2C::scan(address);
	fprintf(stdout, "%d %d\n", address, result);
    }
    fprintf(stdout, "Done.\n");

    while (1) {
    };
}
