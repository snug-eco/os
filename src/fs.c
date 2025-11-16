
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


struct fs_header
{
    uint8_t   flag; //header meta data
    char      name[255]; //ascii encoded name
    uint32_t  hash; //hash of name for faster seeking
    uint32_t  size; //size of file content
} __attribute__((packed));

typedef struct fs_header* fs_file_t;


uint32_t* fs_open(fs_file_t f)
{
    return (uint32_t*)(f + sizeof(struct fs_header));
}


bool fs_check_valid(fs_file_t f)
{
    uint8_t flag = f->flag;
    if ((flag >> FS_FLAG_MUST_ONE     ) & 1 != 1) return false;
    if ((flag >> FS_FLAG_MUST_ZERO    ) & 1 != 0) return false;
    if ((flag >> FS_FLAG_VALID_HEADER ) & 1 != 1) return false;

    return true;
}


//compute base address of next block,
//ignores active state.
fs_file_t fs_next(fs_file_t f)
{
    return f->size + sizeof(struct fs_header_t);
}


/*https://stackoverflow.com/questions/7666509/hash-function-for-string*/
uint32_t fs_hash_dj2(unsigned char* name)
{
    uint32_t hash = 5381;
    char c;

    while (c = *str++)
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
        if (doggie->hash != name_hash)  goto next;
        if (strcmp(doggie->name, name)) goto next;

        //file good
        return doggie;

    next:
        doggie = fs_next(doggie);
    }

    //over-run
    return NULL;
}

bool fs_exists(char* name)
{
    return (fs_seek(name) != NULL);
}

fs_file_t fs_final()
{
    //walkies UwU
    fs_file_t kitty = (fs_file_t)0; // *vine boom*
    
    //kitty goes where it wants to :3
    while (!fs_check_valid(kitty))
        kitty = fs_next(kitty);

    return kitty;
}


fs_file_t fs_create(char* name, uint32_t size)
{
    fs_file_t f = fs_final();

    f->flag = 0;
    f->flag |= (1 << FS_FLAG_MUST_ONE); //duh
    f->flag |= (1 << FS_FLAG_VALID_HEADER); //quoque duh
    f->flag |= (1 << FS_FLAG_FILE_ACTIVE);

    f->flag &= ~(1 << FS_FLAG_MUST_ZERO); //pragmatics

    strcpy(f->name, name);
    f->hash = fs_hash_dj2(name);
    f->size = size;

    return f;
}

void fs_delete(fs_file_t f)
{
    //wow, how simple
    f->flag &= ~(1 << FS_FLAG_FILE_ACTIVE);
}


void fs_collect()
{
    kdebug("fs_collect(): not implemented.\n");
}


