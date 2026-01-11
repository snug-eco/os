#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KDEBUG

#include "term.c"
#include "sd.c"
#include "fs.c"
#include "vm.c"

//async normal speed -> f_osc / (16 * BAUD) + 1
//   8MHz / (16 * 500k) - 1
// = 500_000Hz / 500_000 - 1
//~= 1 - 1 = 0
#define BAUD_SCALAR 0 //0.5 Mbaud


int main(void)
{
    //unset clock prescaler
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

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
