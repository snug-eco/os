#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

#define KLOG

#include "term.c"
#include "sd.c"
#include "fs.c"
#include "vm.c"

//async normal speed -> f_osc / (16 * BAUD) + 1
//   8MHz / (16 * 500k) - 1
// = 500_000Hz / 500_000 - 1
//~= 1 - 1 = 0
#define BAUD_SCALAR 0 //0.5 Mbaud
#define ROOT_BIN "bin.init" //launched on startup

int main(void)
{
    //unset clock prescaler
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

    //terminal
    term_init(BAUD_SCALAR);
    kprint("Terminal operating at 0.5 Mbaud.\n\r");

    //sd card
    sd_init();

    if (fs_exists(ROOT_BIN))
    {
        kprint("Root binary '" ROOT_BIN "' found, launching...\n\r");
        fs_file_t f = fs_seek(ROOT_BIN);
        vm_launch(f);
        kprint("Root binary launched.\n\r");

        kprint("Bringing up task swapper. See you on the other side...\n\r");
        while(vm_pass());

        kprint("Task swapper exited. System going into shutdown NOW.\n\r");
        kprint("Flushing disk caches...\n\r");
        sd_flush();
        kprint("Synchronized.\n\r");

    }
    else
        kprint("No root binary '" ROOT_BIN "' found. Cannot boot, system stalled.\n\r");

    kprint("System is down. Reset to reboot.\n\r");
    return 0;
}
