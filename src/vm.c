
//4 concurrent programs
#define VM_N_PROC 4

char vm_term_in_cache;

typedef uint32_t vint_t;

//this pcb shall not exceed 1k of memory
struct vm_proc
{
    bool active;
    bool term_in_ready;

    sd_addr_t binary;
    uint16_t counter;
    uint16_t size;

    uint16_t return_stack[60];
    uint8_t  return_index;

    vint_t working_stack[120];
    uint8_t working_index;

    vint_t var_store[120];

    uint8_t cache[512];
    sd_addr_t cache_block;
    
} vm_procs[VM_N_PROC] = { 0 };

#define VM_HEAP_SIZE 2000
vint_t vm_heap[VM_HEAP_SIZE] = { 0 };

//one-word header
#define VM_HEAP_HEADER_SIZE 1
#define VM_HEAP_GET_LEN(header) ((header & 0x00FF) >> 0)
#define VM_HEAP_GET_PID(header) ((header & 0x0F00) >> 16)
#define VM_HEAP_HEADER(size, pid) (size | (pid << 8))


typedef struct vm_proc* vm_proc_t;
typedef uint8_t vm_pid_t;


vint_t vm_heap_alloc(vm_pid_t id, vint_t words)
{
    //needed words
    uint16_t needed = (words) + VM_HEAP_HEADER_SIZE;

    // *collars and leashes you* let's go for walkies~ 
    vint_t walker = 0;

    vint_t trial = needed;
    vint_t block;
    while (trial > 0) //trial running
    {
        if ((block = vm_heap[walker]))
        { 
            walker += VM_HEAP_GET_LEN(block); //skip block
            trial   = needed                ; //restart trial
        }
        else
        {
            walker++;
            trial--;
        }
    }

    vint_t ptr = walker - needed;
    vm_heap[ptr] = VM_HEAP_HEADER(needed, id);
    return ptr + VM_HEAP_HEADER_SIZE;  // compute and return base address
}

void vm_heap_free(vint_t base)
{
    vint_t ptr = base - VM_HEAP_HEADER_SIZE; // compute block origin
    uint16_t size = VM_HEAP_GET_LEN(vm_heap[ptr]);

    for (uint16_t i = 0; i < size; i++)
        vm_heap[ptr + i] = 0;
}

void vm_heap_free_process(vm_pid_t owner_pid)
{
    //walk entire heap and free all blocks owned by id
    uint16_t zeros_remaining = 0;
    uint16_t addr = 0;

    vint_t block;
    while (addr < VM_HEAP_SIZE)
        if (zeros_remaining > 0)
        {
            vm_heap[addr] = 0;
            zeros_remaining--;
            addr++;
        }
        else if ((block = vm_heap[addr]))
        {
            uint16_t size      = VM_HEAP_GET_LEN(block);
            vm_pid_t block_pid = VM_HEAP_GET_PID(block);

            // block belongs to process
            if (owner_pid == block_pid)
                //parse header and start zeroing
                zeros_remaining = size;

            else //otherwise skip
                addr += size;
        }
        else addr++;
}



vm_pid_t vm_inactive()
{
    for (int i = 0; i < VM_N_PROC; i++)
        if (!vm_procs[i].active)
            return i;

    return VM_N_PROC;
}

vm_proc_t vm_get_proc(vm_pid_t i)
{
    return &vm_procs[i];
}


vm_pid_t vm_launch(fs_file_t f)
{
    vm_pid_t i = vm_inactive(); 
    if (i == VM_N_PROC) kpanic("Maximum process count reached.");

    vm_proc_t p = vm_get_proc(i);
    p->active  = true;
    p->binary  = fs_open(f);
    p->size    = fs_size(f);
    p->counter = 0;
    p->return_index  = 0;
    p->working_index = 0;
    p->cache_block = -1;

    //zero out
    for (int i = 0; i < 120; i++)
        p->var_store[i] = 0;

    return i;
}

void vm_kill(vm_pid_t id)
{
    vm_proc_t p = vm_get_proc(id);
    p->active = false;

    // free all heap block associated with process
    vm_heap_free_process(id);
}

inline uint8_t vm_read(vm_proc_t p)
{
    sd_addr_t address = p->counter + p->binary;
    
    //compute block and index
    uint32_t block = address >> SD_INDEX_BITS;
    uint16_t index = address &  SD_INDEX_MASK;

    //cache invalidate
    if (block != p->cache_block)
    {
        sd_read_block(block, p->cache);
        p->cache_block = block;
    }

    //normal read and advance
    uint8_t byte = p->cache[index];
    p->counter++;
    return byte;
}
inline vint_t vm_pull(vm_proc_t p)
{
    return p->working_stack[--(p->working_index)];
}
inline void vm_push(vm_proc_t p, vint_t x)
{
    p->working_stack[(p->working_index)++] = x;
}


static char* vm_read_quad_string(vint_t addr)
{
    static char buffer[128];

    uint8_t i = 0;
    vint_t word;
    for (; (word = vm_heap[addr + i]); i++)
        buffer[i] = word;

    buffer[i] = '\0';

    return buffer;
}

void vm_run(vm_pid_t id)
{
    vm_proc_t p = vm_get_proc(id);

    for (int i = 0; i < 2000; i++)
    {
        if (p->counter >= p->size) goto die;

        #define push(x) vm_push(p, x)
        #define pull()  vm_pull(p)

        //read string pointer (just address conversion)
        #define rsp(addr) vm_read_quad_string(addr)
         
         

        uint8_t inst = vm_read(p);
        vint_t a, b, x;
        char* s;

        uint32_t addr;
        switch (inst)
        {
            case 0x00: goto die;

            case 0x01: push(pull() + 1); break;
            case 0x02: pull(); break;
            case 0x03:
                a = pull(); b = pull();
                push(a); push(b);
                break;
            case 0x04: x = pull(); push(x); push(x); break;
            case 0x05: push((vint_t)vm_read(p)); break;
            
            case 0x06: push(pull() == pull()); break;
            case 0x07: push(pull() != pull()); break;
            case 0x08: push(pull() < pull()); break;
            case 0x09: push(pull() > pull()); break;

            case 0x0a: 
                p->counter = (vm_read(p) + (vm_read(p) << 8)); 
                break;
            case 0x0b: 
                if (pull()) p->counter = (vm_read(p) + (vm_read(p) << 8)); 
                else p->counter += 2; 
                break;
            case 0x0c:
                p->return_stack[(p->return_index)++] = (p->counter)+2; 
                p->counter = (vm_read(p) + (vm_read(p) << 8)); 
                break;
            case 0x0d: p->counter = p->return_stack[--(p->return_index)]; break;

            case 0x0e: push(p->var_store[vm_read(p)]); break;
            case 0x0f: p->var_store[vm_read(p)] = pull(); break;
            case 0x10: addr = pull(); push(vm_heap[addr]); break;
            case 0x11: addr = pull(); vm_heap[addr] = pull(); break;

            case 0x12: 
                if (p->term_in_ready) 
                    push(vm_term_in_cache);
                else
                { 
                    p->counter--; 
                    goto yield;
                }
                
                p->term_in_ready = false;
                break;

            case 0x13: 
                if (term_send_ready()) 
                    term_send(pull());
                else
                { 
                    p->counter--; 
                    //goto yield;
                }

                break;

            case 0x14: push(pull() + pull()); break;
            case 0x15: x = pull(); push(pull() - x); break;
            case 0x16: push(pull() * pull()); break;
            case 0x17: x = pull(); push(pull() / x); break;

            case 0x18: push(pull() & pull()); break;
            case 0x19: push(pull() | pull()); break;
            case 0x1a: push(pull() ^ pull()); break;
            case 0x1b: x = pull(); push(pull() << x); break;
            case 0x1c: x = pull(); push(pull() >> x); break;
            case 0x1d: push(~pull()); break;
            case 0x1e: push(!pull()); break;

            case 0x1f:
                addr = pull();
                do
                {
                    x = vm_read(p);
                    vm_heap[addr++] = x;
                } while (x != 0);
                break;

            case 0x20: khex32(pull()); break; //goto yield;


            //system instructions
            //sd disk 
            case 0x80: goto yield;
            case 0x81: sd_flush(); break;
            case 0x82:
                push(sd_read_single(pull())); 
                break;
            case 0x83: 
                x = pull();
                sd_write_single(pull(), x);
                break;

            //file system
            case 0x84: push(fs_exists(rsp(pull()))); break;
            case 0x85: push(fs_seek  (rsp(pull()))); break;
            case 0x86: push(fs_open(pull())); break;
            case 0x87: push(fs_next(pull())); break;

            case 0x88:
                x = pull();
                s = rsp(pull());
                push(fs_create(s, x));
                break;
            case 0x89:
                fs_delete(pull());
                break;
            case 0x8a:
                push(fs_size(pull()));
                break;

            //vm
            case 0x8b: 
                x = vm_launch(pull());
                push(x);
                break;
            case 0x8c:
                vm_kill(pull());
                break;
            case 0x8d:
                push(vm_get_proc(pull())->active);
                break;
            
            //io
            case 0x8e:
                push(p->term_in_ready);
                break;
                    
            //heap
            case 0x90:
                push(vm_heap_alloc(id, pull()));
                break;
            case 0x91:
                vm_heap_free(pull());
                break;
                

            default:
                kdebug("Unkown instruction!\n\r");
                khex8(inst);
                kpanic("Crash\n\r");
        }

        #undef pull
        #undef push
    }
    goto yield;

die:
    vm_kill(id);
    
yield:
    return;
}


bool vm_pass()
{
    bool exec = false;

    //execution pass
    for (vm_pid_t id = 0; id < VM_N_PROC; id++)
    {
        vm_proc_t p = vm_get_proc(id);
        if (!p->active) continue;

        vm_run(id);
        exec = true; // execution as taken place
    }

    //term update pass
    if (term_recv_ready())
    {
        vm_term_in_cache = term_recv();

        //new character available.
        //this is signaled to the processes by term_in_ready.
        for (int i = 0; i < VM_N_PROC; i++)
            vm_procs[i].term_in_ready = true;

    }

    return exec;
}








