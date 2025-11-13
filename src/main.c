#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>


int main(void)
{
    DDRA |= (1 << DDA0);
    while (1)
    {
        PORTA ^= (1 << DDA0);
        _delay_ms(2000);
    }

    return 0;
}
