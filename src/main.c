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

    fs_file_t f = fs_seek("hello");
    sd_addr_t addr = fs_open(f);

    uint32_t size = fs_size(f);

    char buffer[100];
    sd_read(addr, &buffer, size);
    buffer[size] = '\0'; //termi

    kdebug("size of hello: ");
    khex8(size);

    kdebug("content: ");
    term_print(buffer);


    fs_delete(f);





    return 0;
}
