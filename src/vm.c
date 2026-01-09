
//8 concurrent programs
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

uint8_t vm_heap[8000] = { 0 };


typedef struct vm_proc* vm_proc_t;
typedef int vm_pid_t;

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

void vm_kill(vm_pid_t i)
{
    vm_proc_t p = vm_get_proc(i);
    p->active = false;
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
    //re-interpret
    uint32_t* ptr = (uintptr_t)addr;

    static char buffer[128];

    for (uint8_t i = 0; ptr[i]; i++)
        buffer[i] = ptr[i];

    return buffer;
}

void vm_run(vm_proc_t p)
{
    for (int i = 0; i < 2000; i++)
    {
        if (p->counter >= p->size) goto die;

        #define push(x) vm_push(p, x)
        #define pull()  vm_pull(p)

        //read string pointer (just address conversion)
        //!TODO redefine
        #define rsp(addr) vm_read_quad_string(addr)
         
         

        uint8_t inst = vm_read(p);
        uint8_t a, b, x;
        char* s;

        uint32_t* addr;
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
            case 0x10: addr = (uint32_t*)pull(); push(*addr); break;
            case 0x11: addr = (uint32_t*)pull(); *addr = pull(); break;

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
                    goto yield;
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

            case 0x1e: khex32(pull()); goto yield;
            case 0x1f:
                a = pull();
                do
                {
                    x = vm_read(p);
                    //p->data_store[a++] = x;
                } while (x != 0);
                break;

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
            case 0x85: //x = pull(); r32(pull()) = (fs_seek(rsp(x))); break;
            case 0x86: a = pull(); a = fs_open(a); break;
            case 0x87: a = pull(); a = fs_next(a); break;

            case 0x88:
                x = pull();
                s = rsp(pull());
                //r32(pull()) = fs_create(s, x);
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
    p->active = false;
    
yield:
    return;
}


bool vm_pass()
{
    bool exec = false;

    //execution pass
    for (int i = 0; i < VM_N_PROC; i++)
    {
        vm_proc_t p = &vm_procs[i];
        if (!p->active) continue;

        vm_run(p);
        exec = true;
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








