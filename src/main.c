#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KDEBUG

#include "term.c"
#include "sd.c"

//#define BAUD 9600UL
//#define BAUD_SCALAR (F_CPU / (BAUD * 16))
#define BAUD_SCALAR 12

uint8_t test_block1[512] = { 0 };
uint8_t test_block2[512] = { 0 };


int main(void)
{
    for (int i = 0; i < 10; i++)
        test_block1[i] = i;
    
    //terminal
    term_init(BAUD_SCALAR);

    //sd card
    sd_init();

    sd_write_block(0, test_block1);
    sd_read_block (0, test_block2);

    for (int i = 0; i < 10; i++)
        khex(test_block2[i]);




    return 0;
}
