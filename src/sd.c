// this driver expects a high capacity sd card,
// one with block addressing.

// this file is heavily commented
// because the spi driver is pretty opaque.

// DO NOT TOUCH THIS FILE (unless you know what you're doing)
// this sd card driver contains sensitive code, which could
// if changed even slightly, wreck user data.

#include <avr/io.h>

#include <stdint.h>
#include <stdbool.h>


typedef uint32_t sd_addr_t;


//commands
#define SD_CMD0   0x0
#define SD_CMD8   0x8
#define SD_CMD17  0x11
#define SD_CMD24  0x18
#define SD_CMD55  0x37
#define SD_ACMD41 0x29
#define SD_CMD58  0x3A

#define sd_panic(msg) kpanic("[SD] " msg "\n\r\tTry power cycling the SD-Card and device.\n\r")
#define sd_print(msg) kprint("[SD] " msg "\n\r")


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
    // at f_osc = 8MHz, f_osc / 128 < 400kHz, such that sd card an initialize
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
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

    //sd_print("Command transmission time-out.");

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

    SD_CS_OFF(); 
    sd_print("Card initialized.");

    //switching into full speed.
    // f_osc / 2 which defaults to ~0.5MHz
    sd_print("Switching into full speed.");
    SPCR &= ~((1 << SPR1) | (1 << SPR0)); //unset clock diviers
    SPSR |= (1 << SPI2X);
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
static uint8_t  sd_cache[512];
static uint32_t sd_cache_block = -1;
static bool     sd_cache_need_writeback = false;

#define SD_INDEX_BITS 9
#define SD_INDEX_SIZE (1 << SD_INDEX_BITS)
#define SD_INDEX_MASK (SD_INDEX_SIZE - 1)

void sd_flush()
{
    //if write has happened during cache cycle,
    //write-back to card has to happen.
    if (sd_cache_need_writeback)
        sd_write_block(sd_cache_block, sd_cache);

    sd_cache_need_writeback = false;
}

void sd_recache(uint32_t block)
{
    //flush cache block to save writes.
    sd_flush();

    //read new block
    sd_read_block(block, sd_cache);
    sd_cache_block = block;
}


uint8_t sd_read_single(sd_addr_t address)
{
    //compute block and index numbers
    uint32_t block = address >> SD_INDEX_BITS;
    uint16_t index = address &  SD_INDEX_MASK;

    //cache invalidate
    if (block != sd_cache_block)
        sd_recache(block);

    return sd_cache[index];
}

void sd_write_single(sd_addr_t address, uint8_t value)
{
    //compute block and index numbers
    uint32_t block = address >> SD_INDEX_BITS;
    uint16_t index = address &  SD_INDEX_MASK;

    //cache invalidate
    if (block != sd_cache_block)
        sd_recache(block);

    //written during cache cycle,
    //therefore as to write-back during recache.
    sd_cache_need_writeback = true; 

    sd_cache[index] = value;
}


void sd_read(sd_addr_t address, uint8_t* dst, uint32_t n)
{
    for (int i = 0; i < n; i++)
        dst[i] = sd_read_single(address + i);
}
void sd_write(sd_addr_t address, uint8_t* src, uint32_t n)
{
    for (int i = 0; i < n; i++)
        sd_write_single(address + i, src[i]);
}





