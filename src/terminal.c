
//terminal io for uart 1

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
