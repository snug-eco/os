#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KDEBUG

#include "term.c"
#include "sd.c"
#include "fs.c"
#include "vm.c"

//#define BAUD 9600UL
//#define BAUD_SCALAR (F_CPU / (BAUD * 16))
#define BAUD_SCALAR 12


int main(void)
{
    //terminal
    term_init(BAUD_SCALAR);

    //sd card
    sd_init();

    if (fs_exists("bin.init"))
    {
        fs_file_t f = fs_seek("bin.init");
        vm_launch(f);
    }
    else
        kdebug("No init file found.");


    while(vm_pass());

    kdebug("All processes died, shutting down system.");


    return 0;
}
