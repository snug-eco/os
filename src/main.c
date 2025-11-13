#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

#include "terminal.c"

#define BAUD 300
#define BAUD_SCALAR (F_CPU / (16 * BAUD))

int main(void)
{
    term_init(BAUD_SCALAR);

    char* ptr = "hello world\n";

    for (; *ptr; ptr++)
    {
        term_send(*ptr);
    }

    return 0;
}
