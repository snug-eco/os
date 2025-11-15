// this driver expects a high capacity sd card,
// one with block addressing.

// this file is heavily commented
// because the spi driver is pretty opaque.

#include <avr/io.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdbool.h>




//commands
#define SD_CMD0   0x0
#define SD_CMD8   0x8
#define SD_CMD17  0x11
#define SD_CMD24  0x18
#define SD_CMD55  0x37
#define SD_ACMD41 0x29
#define SD_CMD58  0x3A

#define sd_panic(msg) kpanic("[SD] " msg "\n\tTry power cycling the SD-Card and device.\n")
#define sd_print(msg) kprint("[SD] " msg "\n")


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


//remember: active low
void SD_CS_ON()
{
    PORTB &= ~(1 << PB0);
    sd_spi_send(0xFF);
}
void SD_CS_OFF()
{
    PORTB |=  (1 << PB0);
    sd_spi_send(0xFF);
}





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



uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    //make sure card is selected
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

    sd_print("Command transmission time-out.");

done:
    sd_spi_send(0xFF);
    return resp;
}

uint8_t sd_acmd(uint8_t cmd, uint32_t arg)
{
    sd_cmd(SD_CMD55, 0);
    return sd_cmd(cmd, arg);
}



void sd_init(void) 
{
    sd_print("Initializing... ");
    sd_spi_init();
    SD_CS_OFF();

    //send 74+ spi clock cycles to allow sd card to initialize.
    for (int i = 0; i < 10; i++) sd_spi_send(0xFF);

    SD_CS_ON();

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
    for (int i = 0; i < 100; i++)
    {
        resp = sd_acmd(SD_ACMD41, 0x40000000);
        if (resp == 0) break; //ready success
    }
    if (resp != 0) sd_panic("Timeout, Card cannot be brought into ready state.");


    resp = sd_cmd(SD_CMD58, 0); //read ocr.
    if (resp != 0) sd_panic("OCR read error.");
    
    bool high_cap = (sd_spi_recv() & 0x40) != 0;
    sd_spi_recv(); //don't care
    sd_spi_recv(); //don't care
    sd_spi_recv(); //don't care

    if (!high_cap) sd_panic("Card is not SDHC or SDHX, Card is unsupported."); //only high capacity cards.

    sd_print("Card initialized.");
    SD_CS_OFF(); 
}




void sd_read_block(uint32_t block, uint8_t* buffer)
{
    sd_cmd(SD_CMD17, block);

    //catch data token
    while (sd_spi_recv() != 0xFE); 

    for (uint16_t i = 0; i < 512; i++)
        buffer[i] = sd_spi_recv();

    sd_spi_recv(); //dummy crc
    sd_spi_recv(); //dummy crc
}

void sd_write_block(uint32_t block, uint8_t* buffer)
{
    sd_cmd(SD_CMD24, block);

    //throw data token
    sd_spi_send(0xFE);

    for (uint16_t i = 0; i < 512; i++)
        sd_spi_send(buffer[i]);

    sd_spi_send(0xFF); //dummy crc
    sd_spi_send(0xFF); //dummy crc
    
    uint8_t resp = sd_spi_recv(0xFF);
    bool accepted = (resp & 0x1F) == 0x05;
    if (!accepted) sd_panic("Block write error.");

    //wait for flash to complete
    while (sd_spi_recv() == 0);
}

//generic read/write cache
static uint8_t  sd_block_cache[512];
static uint32_t sd_block_cache_index = -1;

uint8_t sd_read(uint32_t req_address)
{
    uint32_t req_block_index  = req_address >> 9;
    uint16_t req_block_offset = req_address & (512 - 1);

    //block invalidate
    if (req_block_index != sd_block_cache_index)
        sd_read_block(req_block_index, sd_block_cache);

    sd_block_cache_index = req_block_index;

    return sd_block_cache[req_block_offset];
}

void sd_write(uint32_t req_address, uint8_t value)
{
    
}





