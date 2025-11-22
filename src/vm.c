
//8 concurrent programs
#define VM_N_PROC 8

char vm_term_in_cache;

//this pcb shall not exceed 1k of memory
struct vm_proc
{
    bool active;
    bool term_in_ready;

    sd_addr_t binary;
    uint16_t counter;
    uint16_t size;

    uint8_t return_stack[256];
    uint8_t return_index;

    uint8_t working_stack[128];
    uint8_t working_index;

    uint8_t  var_store[128];
    uint8_t data_store[256];
    
} vm_procs[VM_N_PROC] = { 0 };

typedef struct vm_proc* vm_proc_t;

vm_proc_t vm_inactive()
{
    for (int i = 0; i < VM_N_PROC; i++)
        if (!vm_procs[i].active)
            return &vm_procs[i];

    return NULL;
}



void vm_launch(fs_file_t f)
{
    vm_proc_t p = vm_inactive(); 
    if (!p) kpanic("Maximum process count reached.");

    p->active  = true;
    p->binary  = fs_open(f);
    p->size    = fs_size(f);
    p->counter = 0;
    p->return_index  = 0;
    p->working_index = 0;
}

inline uint8_t vm_read(vm_proc_t p)
{
    uint8_t byte = sd_read_single(p->counter + p->binary);
    p->counter++;
    return byte;
}
uint8_t vm_pull(vm_proc_t p)
{
    return p->working_stack[--(p->working_index)];
}
void vm_push(vm_proc_t p, uint8_t x)
{
    p->working_stack[(p->working_index)++] = x;
}


void vm_run(vm_proc_t p)
{
    for (int i = 0; i < 2000; i++)
    {
        if (p->counter >= p->size) goto die;

        #define push(x) vm_push(p, x)
        #define pull()  vm_pull(p)

        uint8_t inst = vm_read(p);
        uint8_t a, b, x;
        switch (inst)
        {
            case 0x00: goto die;

            case 0x01: push(pull() + 1); break;
            case 0x02: pull();
            case 0x03:
                a = pull(); b = pull();
                push(a); push(b);
                break;
            case 0x04: x = pull(); push(x); push(x); break;
            case 0x05: push(vm_read(p)); break;
            
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
            case 0x10: push(p->data_store[pull()]); break;
            case 0x11: x = pull(); p->data_store[pull()] = x; break;

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

            case 0x1e: khex8(pull()); break;
            case 0x1f:
                a = pull();
                do
                {
                    x = vm_read(p);
                    p->data_store[a++] = x;
                } while (x != 0);
                break;

            //system instructions
            case 0x80: goto yield;

            default:
                kdebug("Unkown instruction!\n");
                khex8(inst);
                kpanic("Crash\n");
        }

        #undef pull
        #undef push
    }

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








