
//terminal io for uart 1

#include <avr/io.h>
#include <util/delay.h>

void term_init(unsigned int baud)
{
    //set baud rate
    UBRR1H = (unsigned char)(baud >> 8);
    UBRR1L = (unsigned char)(baud >> 0);

    //enable rx tx
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);

    // 8 bit, no parity
    UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);
}


void term_send(unsigned char byte)
{
    //poll for transmit buffer empty
    while (!(UCSR1A & (1<<UDRE1)));

    UDR1 = byte;
}

void term_clear()
{
    for (int i = 0; i < 30; i++)
        term_send('\n');

}


void term_print(char* str)
{
    for (; *str; str++)
        term_send(*str);
}

void kprint(char* str)
{
    term_print("[KERNEL] ");
    term_print(str);
}

void kdebug(char* str)
{
    #ifdef KDEBUG
        term_print("[DEBUG] ");
        term_print(str);
    #endif
}

void khex(uint8_t byte)
{
    term_print("[HEX] 0x");
    static char digit[16] = "0123456789ABCDEF";
    term_send(digit[(byte >> 4) & 0xF]);
    term_send(digit[(byte >> 0) & 0xF]);
    term_send('\n');
}

void kpanic(char* str)
{
    term_print("[PANIC] ");
    term_print(str);

    while (1) _delay_ms(1000);
}
