
//terminal driver for uart 1.

#include <avr/io.h>
#include <util/delay.h>


void term_send(unsigned char byte)
{
    while (!(UCSR1A & (1<<UDRE1)));
    UDR1 = byte;
}

unsigned char term_recv()
{
    while (!(UCSR1A & (1<<RXC1)));
    return UDR1;
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

void term_init(unsigned int baud)
{
    //set baud rate
    UBRR1H = (unsigned char)(baud >> 8);
    UBRR1L = (unsigned char)(baud >> 0);

    //enable rx tx
    UCSR1B = (1<<RXEN1)|(1<<TXEN1);

    // 8 bit, no parity
    UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);

    term_clear();
    kprint("[TERM] Terminal UART 1 connection initialized.\n");
}


void kdebug(char* str)
{
    #ifdef KDEBUG
        term_print("[DEBUG] ");
        term_print(str);
    #endif
}

static char khex_digit[16] = "0123456789ABCDEF";
void khex8(uint8_t byte)
{
    term_print("[HEX8] 0x");
    for (int off = 8; off; off -= 4)
        term_send(khex_digit[(byte >> (off - 4)) & 0xF]);
    term_send('\n');
}

void khex32(uint32_t word)
{
    term_print("[HEX32] 0x");
    for (int off = 32; off; off -= 4)
        term_send(khex_digit[(word >> (off - 4)) & 0xF]);
    term_send('\n');
}

void kpanic(char* str)
{
    term_print("[PANIC] ");
    term_print(str);

    while (1) _delay_ms(1000);
}
