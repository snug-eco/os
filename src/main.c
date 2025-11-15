#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KDEBUG

#include "term.c"
#include "sd.c"

//#define BAUD 9600UL
//#define BAUD_SCALAR (F_CPU / (BAUD * 16))
#define BAUD_SCALAR 12


int main(void)
{
    term_init(BAUD_SCALAR);
    term_clear();
    kprint("Terminal UART connection initialized.\n");

    sd_init();
    kprint("sd card initialized.\n");





    return 0;
}
