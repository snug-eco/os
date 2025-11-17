#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KDEBUG

#include "term.c"
#include "sd.c"
#include "fs.c"

//#define BAUD 9600UL
//#define BAUD_SCALAR (F_CPU / (BAUD * 16))
#define BAUD_SCALAR 12

int main(void)
{
    //terminal
    term_init(BAUD_SCALAR);

    //sd card
    sd_init();

    fs_create("test1", 10);
    fs_create("test2", 20);
    fs_create("test3", 30);


    return 0;
}
