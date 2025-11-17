
#include <avr/io.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// a valid header flag must at least xxxxx101
enum fs_flag_fields
{
    FS_FLAG_RESERVED0 = 0,
    FS_FLAG_RESERVED1    ,
    FS_FLAG_RESERVED2    ,
    FS_FLAG_RESERVED3    ,
    FS_FLAG_FILE_ACTIVE  ,
    FS_FLAG_MUST_ONE     , //zero-out protect
    FS_FLAG_MUST_ZERO    , // one-out protect
    FS_FLAG_VALID_HEADER 
};


struct fs_header_s
{
    uint8_t   flag; //header meta data
    char      name[255]; //ascii encoded name
    uint32_t  hash; //hash of name for faster seeking
    uint32_t  size; //size of file content
} __attribute__((packed));


typedef sd_addr_t fs_file_t;


static struct fs_header_s fs_header;

void fs_read_header(fs_file_t f)
{
    sd_read(
        f, 
        (uint8_t*)&fs_header, 
        sizeof(struct fs_header_s)
    );
}

void fs_write_header(fs_file_t f)
{
    sd_write(
        f, 
        (uint8_t*)&fs_header, 
        sizeof(struct fs_header_s)
    );
    sd_flush();
}






sd_addr_t fs_open(fs_file_t f)
{
    return (sd_addr_t)(f + sizeof(struct fs_header_s));
}


bool fs_check_valid(fs_file_t f)
{
    fs_read_header(f);
    uint8_t flag = fs_header.flag;
    kdebug("flags:");
    khex8(flag);
    if (((flag >> FS_FLAG_MUST_ONE     ) & 1) != 1) return false;
    if (((flag >> FS_FLAG_MUST_ZERO    ) & 1) != 0) return false;
    if (((flag >> FS_FLAG_VALID_HEADER ) & 1) != 1) return false;

    kdebug("valid\n");
    return true;
}


//compute base address of next block,
//ignores active state.
fs_file_t fs_next(fs_file_t f)
{
    fs_read_header(f);

    return (fs_file_t)(
        f
        + sizeof(struct fs_header_s)
        + fs_header.size
    );
}


/*https://stackoverflow.com/questions/7666509/hash-function-for-string*/
uint32_t fs_hash_dj2(unsigned char* name)
{
    uint32_t hash = 5381;
    char c;

    while (c = *name++)
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    
    return hash;
}


fs_file_t fs_seek(char* name)
{
    uint32_t name_hash = fs_hash_dj2(name);

    //walkies UwU
    fs_file_t doggie = (fs_file_t)0; // *vine boom*
    
    while (fs_check_valid(doggie))
    {
        fs_read_header(doggie);
        if (fs_header.hash != name_hash)  goto next;
        if (strcmp(fs_header.name, name)) goto next;

        return doggie;

    next:
        doggie = fs_next(doggie);
    }

    //over-run
    return 0;
}

bool fs_exists(char* name)
{
    return (fs_seek(name) != 0);
}

fs_file_t fs_final()
{
    //walkies UwU
    fs_file_t kitty = (fs_file_t)0; // *vine boom*
    
    //kitty goes where it wants to :3
    while (fs_check_valid(kitty))
    {
        kdebug("before kitty ");
        khex32(kitty);
        kitty = fs_next(kitty);
        kdebug("after  kitty ");
        khex32(kitty);
    }

    return kitty;
}


fs_file_t fs_create(char* name, uint32_t size)
{
    kdebug("fs_create: start\n");

    fs_file_t f = fs_final();
    khex32(f);

    kdebug("fs_create: final found\n");

    fs_read_header(f);

    fs_header.flag = 0;
    fs_header.flag |= (1 << FS_FLAG_MUST_ONE); //duh
    fs_header.flag |= (1 << FS_FLAG_VALID_HEADER); //quoque duh
    fs_header.flag |= (1 << FS_FLAG_FILE_ACTIVE);

    fs_header.flag &= ~(1 << FS_FLAG_MUST_ZERO); //pragmatics

    strcpy(fs_header.name, name);
    fs_header.hash = fs_hash_dj2(name);
    fs_header.size = size;

    fs_write_header(f);
    kdebug("fs_create: header written, file created\n");
    return f;
}

void fs_delete(fs_file_t f)
{
    fs_read_header(f);

    //wow, how simple
    fs_header.flag &= ~(1 << FS_FLAG_FILE_ACTIVE);

    fs_write_header(f);
}


void fs_collect()
{
    kdebug("fs_collect(): not implemented.\n");
}


