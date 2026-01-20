
//terminal driver for uart 1.

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define term_send_ready() (UCSR1A & (1<<UDRE1))
#define term_recv_ready() (UCSR1A & (1<<RXC1))


void term_send(unsigned char byte)
{
    while (!term_send_ready());
    UDR1 = byte;
}

unsigned char term_recv()
{
    while (!term_recv_ready());
    return UDR1;
}


void term_clear()
{
    term_send('\r');
    for (int i = 0; i < 30; i++)
        term_send('\n');
}

void term_print(const char* str)
{
    for (; *str; str++)
        term_send(*str);
}

void term_print_prog(const char* pstr)
{
    char c;
    while((c = pgm_read_byte(pstr++))) 
        term_send(c);
}

void kprint(const char* str)
{
    term_print_prog(PSTR("[KERNEL] "));
    term_print_prog(str);
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
    kprint(PSTR("[TERM] Terminal connection initialized.\n\r"));
}


#define klog(s) _klog(PSTR(s "\n\r"))
void _klog(const char* str)
{
    #ifdef KLOG
        term_print_prog(PSTR("[LOG] "));
        term_print_prog(str);
    #endif
}


void kdebug(const char* str)
{
    term_print_prog(PSTR("[DEBUG] "));
    term_print_prog(str);
}


const char khex_digit[16] PROGMEM = "0123456789ABCDEF";
void khex8(uint8_t byte)
{
    term_print_prog(PSTR("[HEX8] 0x"));
    for (int off = 8; off; off -= 4)
        term_send(pgm_read_byte(&khex_digit[(byte >> (off - 4)) & 0xF]));
    term_send('\n');
    term_send('\r');
}

void khex32(uint32_t word)
{
    term_print_prog(PSTR("[HEX32] 0x"));
    for (int off = 32; off; off -= 4)
        term_send(pgm_read_byte(&khex_digit[(word >> (off - 4)) & 0xF]));
    term_send('\n');
    term_send('\r');
}

void kpanic(const char* str)
{
    term_print_prog(PSTR("[PANIC] "));
    term_print_prog(str);

    while (1) _delay_ms(1000);
}
