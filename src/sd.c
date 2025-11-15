// this driver expects a high capacity sd card,
// one with block addressing.

// this file is heavily commented
// because the spi driver is pretty opaque.

#include <avr/io.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdbool.h>



//remember: active low
#define SD_CS_ON()  (PORTB &= ~(1 << PB0))
#define SD_CS_OFF() (PORTB |=  (1 << PB0))

//commands
#define SD_CMD0   0x0
#define SD_CMD8   0x8
#define SD_CMD17  0x11
#define SD_CMD24  0x18
#define SD_CMD55  0x37
#define SD_ACMD41 0x29
#define SD_CMD58  0x3A

void sd_spi_init(void)
{
    //set io modes
    DDRB |=  (1 << PB5); //MOSI
    DDRB |=  (1 << PB7); //SCK
    DDRB |=  (1 << PB0); //CS
    DDRB |=  (1 << PB4); //SS (not used, must still be set)
    DDRB &= ~(1 << PB6); // MISO
    
    PORTB |= (1 << PB4);
    
    // enable spi master mode, set clock rate f_osc/16
    // at f_osc = 1MHz, f_osc / 16 < 400kHz, such that sd card an initialize
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0) | (1 << SPR1);
    SPCR &= ~(1 << SPI2X); //clear double speed
    SD_CS_OFF();
}


void sd_spi_send(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
}

uint8_t sd_spi_recv()
{
    sd_spi_send(0xFF);
    uint8_t data = SPDR;

    return data;
}


uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    SD_CS_ON();

    sd_spi_send(cmd | 0x40);
    sd_spi_send((arg >> 24) & 0xFF);
    sd_spi_send((arg >> 16) & 0xFF);
    sd_spi_send((arg >>  8) & 0xFF);
    sd_spi_send((arg >>  0) & 0xFF);

    //crc
    uint8_t crc = 0xFF;
    if (cmd == SD_CMD0) crc = 0x95;
    if (cmd == SD_CMD8) crc = 0x87;
    sd_spi_send(crc);


    //await response
    uint8_t resp;
    for (uint16_t i = 0; i < 0xFFF; i++)
    {
        resp = sd_spi_recv();
        if (!(resp & 0x80)) goto done;
    }

    kdebug("sd_cmd: transmission time out\n");

done:
    SD_CS_OFF();
    sd_spi_send(0xFF);
    return resp;
}

uint8_t sd_acmd(uint8_t cmd, uint32_t arg)
{
    sd_cmd(SD_CMD55, 0);
    return sd_cmd(cmd, arg);
}


#define sd_panic(msg) kpanic("[SD-Card subsystem] " msg "\n\tTry power cycling the SD-Card and device.\n")
#define sd_print(msg) kprint("[SD-Card subsystem] " msg "\n")

void sd_init(void) 
{
    kdebug("sd_init\n");
    sd_spi_init();
    SD_CS_OFF();

    //send 74+ spi clock cycles to allow sd card to initialize.
    for (int i = 0; i < 10; i++) sd_spi_send(0xFF);

    SD_CS_ON(); //start transaction

    //set spi mode
    uint8_t resp;
    for (int i = 0; i < 1000; i++)
    {
        resp = sd_cmd(SD_CMD0, 0); //set sd to spi mode
        if (resp == 1) break; //init success
    }
    if (resp != 1) sd_panic("Unable to put Card into SPI mode.");
    

    //voltage and version check
    resp = sd_cmd(SD_CMD8, 0x1AA);
    if (resp != 1) sd_panic("Unsupported Card.");
    
    
    //wait for card to be ready
    kdebug("sd_init: waiting for ready state... ");
    for (int i = 0; i < 100; i++)
    {
        resp = sd_acmd(SD_ACMD41, 0x40000000);
        khex(resp);
        if (resp == 0) break; //ready success
    }
    if (resp != 0) sd_panic("Timeout, Card cannot be brought into ready state.");
    kdebug("good\n");


    resp = sd_cmd(SD_CMD58, 0); //read ocr.
    if (resp != 0) sd_panic("OCR read error.");
    
    kdebug("sd_init: ocr read\n");
    
    bool high_cap = (sd_spi_recv() & 0x40) != 0;
    sd_spi_recv(); //don't care
    sd_spi_recv(); //don't care
    sd_spi_recv(); //don't care

    if (!high_cap) sd_panic("Card is not SDHC or SDHX, Card is unsupported."); //only high capacity cards.

    sd_print("Card initialized.");
    
    SD_CS_OFF(); //transaction complete
}


/*

void sd_read_block(uint32_t block, uint8_t* buffer)
{
    SD_CS_ON();
    sd_cmd(SD_CMD17, block);

    //catch data token
    while (sd_spi_transfer(0xFF) != 0xFE); 

    for (uint16_t i = 0; i < 512; i++)
        buffer[i] = sd_spi_transfer(0xFF);

    sd_spi_transfer(0xFF); //dummy crc
    sd_spi_transfer(0xFF); //stream terminate
    SD_CS_OFF();
}

void sd_write_block(uint32_t block, uint8_t* buffer)
{
    SD_CS_ON();
    sd_cmd(SD_CMD24, block);

    //throw data token
    sd_spi_transfer(0xFE);

    for (uint16_t i = 0; i < 512; i++)
        sd_spi_transfer(buffer[i]);

    sd_spi_transfer(0xFF); //dummy crc
    sd_spi_transfer(0xFF); //stream terminate
    
    uint8_t resp = sd_spi_transfer(0xFF);
    bool accepted = ((resp & 0x1F) != 0x05);

    //wait till not busy
    while (sd_spi_transfer(0xFF) == 0);

}



*/
