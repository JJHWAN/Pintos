#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/syscall.h"
#include <stdlib.h>

extern struct lock filesys_lock;

struct thread* get_child_process(int pid);
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

// stack growth를 구현하는 함수
bool expand_stack(void* addr);


struct thread* get_child_process(int pid){

  // parent에서 list child를 검색해서 return
  struct list_elem *ptr;
  struct thread* t = thread_current();
  struct thread* temp = NULL;
  
  for (ptr = list_begin(&(t->child)); ptr != list_end(&(t->child)); ptr = list_next(ptr)){
    temp = list_entry(ptr, struct thread, child_elem);
    if(pid==temp->tid){
      return temp;
    }
  }

  return NULL;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  struct thread* cur = thread_current();

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // need to parse? 
  char cmd_copy[129], *next_ptr, *filename_parsed;

  strlcpy(cmd_copy, file_name, strlen(file_name)+1);
  filename_parsed = strtok_r(cmd_copy, " ", &next_ptr);

  tid = thread_create (filename_parsed, PRI_DEFAULT, start_process, fn_copy);
  sema_down(&(cur->load));
  //printf("to the parent thread\n");
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  
  if(cur->load_sucess==-1){
    return -1;
  }
  
  return tid; // exit_status return인데
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  spt_init(&thread_current()->spt);
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  //printf("load 끝!, sucess : %d\n", success);
  // 수정해야함!!
  sema_up(&((thread_current()->parent)->load));
  //for(int i=0;i<100;i++)
  //printf("end of sema_up\n");
  /* If load failed, quit. */

  palloc_free_page (file_name);
  if (!success){ 
    thread_current()->parent->load_sucess = -1;
    thread_exit ();
  }

  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  
  struct list_elem* ptr;
  struct thread* t = NULL;
  int exit_status;
  // t = thread_current();
  t = get_child_process((int)child_tid);
  // get thread from cur->child
  // if exist return thread *
  // else return NULL
  
  if(t!=NULL){
    // printf("in process wait and found thread!!\n");
    // if found such child process
    // wait for child to end
    sema_down(&(t->exit));
    // get exit status
    exit_status = t->exit_status;
    // printf("exit_status:%d\n", exit_status);
    // remove from child, cause child process exited.
    list_remove(&(t->child_elem));
    sema_up(&(t->mem));
    //for(int i=0;i<100;i++)
    //  printf("exit_status : %d\n", exit_status);
    return exit_status;
  }   
  
  return -1;
   
}


/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *t = thread_current ();
  uint32_t *pd;

  // remove mmap_list for thread
  //printf("here?\n");
  munmap(-1);

  // remove vm_entry 
  spt_destroy(&(t->spt));

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = t->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      //printf("%s: exit(%d)\n", t->name, t->status);
      t->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

    // Semapore up
    sema_up(&(t->exit));
    sema_down(&(t->mem));
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static bool load_segment_vm (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);


/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i=0, num_arg;

  //printf("##filename in load: %s\n", file_name);

  // copy filename
  char cmd_copy[129], cmd_copy2[129];
  strlcpy(cmd_copy, file_name, strlen(file_name)+1);
  strlcpy(cmd_copy2, file_name, strlen(file_name)+1);

  // need an array for temporary save for stack
  char *next_ptr, *arg_ptr, **arguments;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  // TODD: parse file name
  // why?

  arg_ptr = strtok_r(cmd_copy, " ", &next_ptr); i++;
  while(arg_ptr!=NULL){
    arg_ptr = strtok_r(NULL, " ", &next_ptr);
    i++;
  }
  num_arg = i-1;
  //printf("num_arg: %d\n", num_arg);
  arguments = (char**)malloc(sizeof(char*)*num_arg);  

  arg_ptr = strtok_r(cmd_copy2, " ", &next_ptr);
  arguments[0] = arg_ptr;
  //printf("%s\n", arguments[0]);

  for(i=1;i<num_arg;i++){
    arg_ptr = strtok_r(NULL, " ", &next_ptr);
    arguments[i] = arg_ptr;
    //printf("%s\n", arguments[i]);
  }
  lock_acquire(&filesys_lock);
  file = filesys_open (arguments[0]);
  lock_release(&filesys_lock);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  
  //printf("hi");
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  //printf("setup_stack before!\n");
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  //printf("setup_stack done!\n");


  // push arguments[num_arg-1] ~ arguments[0]
  
  int byte_cnt = 0, len;
  uint32_t *argu_addr = (uint32_t*)malloc(sizeof(uint32_t)*num_arg); 

  for(i=num_arg-1;i>=0;i--){  
    len = strlen(arguments[i]);
    *esp = *esp -(len+1); // "echo\0"
    byte_cnt += len+1;
    strlcpy(*esp, arguments[i], len+1); // push to stack
    argu_addr[i] = *esp;
  }

  // make word align (4bytes)
  while(byte_cnt%4!=0){
    *esp = *esp - 1;
    **(uint8_t **)esp = 0;
    byte_cnt++;
  }

  // push NULL
  *esp = *esp -4;
  **(uint32_t **)esp = 0;
  
  // push address of arguments[i]
  for(i=num_arg-1;i>=0;i--){ 
    *esp = *esp - 4;
    **(uint32_t **) esp = argu_addr[i];
  }
  
  // push address of arguments
  *esp = *esp - 4;
  **(uint32_t**)esp = *esp+4;

  // push num_arg
  *esp = *esp - 4;
  **(uint32_t**)esp = num_arg;

  // push return address
  *esp = *esp - 4;
  **(uint32_t**)esp = 0;

  //printf("everything done.\n");
  //hex_dump(*esp, *esp, 100, 1);

  free(argu_addr);
  free(arguments);


  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */

static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{

  struct file* open_file = file_reopen (file);
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  file_seek(file, ofs);
  
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      // spt_entry 생성 with malloc
      struct spt_entry *spe =(struct spt_entry*)malloc(sizeof(struct spt_entry));
      if(spe == NULL)
        return false;

      // spt_entry 필드 초기화
      memset (spe, 0, sizeof (struct spt_entry));
      spe->type = VM_BIN;
      spe->file = open_file;
      spe->offset = ofs;
      spe->read_bytes = page_read_bytes;
      spe->zero_bytes = page_zero_bytes;
      spe->writable = writable;
      spe->vaddr = upage;
      spe->is_loaded  = false;
      //printf("spe(vaddr, file) : %X, %p\n", spe->vaddr, spe->file);
      insert_spe (&(thread_current ()->spt), spe);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct page *kpage;
  bool success = false;

  struct spt_entry *spe = (struct spt_entry*)malloc(sizeof(struct spt_entry));
  if(spe == NULL)
    return false;

  kpage = alloc_page_frame (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      kpage->spe = spe;
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage->kaddr, true);
      if (success)
        *esp = PHYS_BASE;
      else{
        free_page (kpage->kaddr);
        free(spe);
        return false;
      } 
    }

    //memset(kpage->spe, 0, sizeof(struct spt_entry));
    spe->type = VM_ANON;
    spe->writable = true;
    spe->is_loaded = true;
    spe->vaddr = pg_round_down(((uint8_t *) PHYS_BASE) - PGSIZE);
    kpage->spe = spe;
    insert_spe(&(thread_current()->spt), kpage->spe);
    add_page_to_lru_list(kpage);
    //printf("setup stack : kpage->spe->vaddr : %p\n", kpage->spe->vaddr);

  return success;
}

bool expand_stack(void* addr){

  // addr 주소를 포함하도록 스택을 확장한다.
  // setup stack과 
  struct page *kpage;
  bool success = false;
  void* vaddr = pg_round_down(addr);

  struct spt_entry *spe = (struct spt_entry*)malloc(sizeof(struct spt_entry));
  if(spe == NULL)
    return false;

  kpage = alloc_page_frame (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      kpage->spe = spe;
      success = install_page (vaddr, kpage->kaddr, true);
      if (!success){
        free_page (kpage->kaddr);
        free(spe);
        return false;
      } 
    }

    spe->type = VM_ANON;
    spe->writable = true;
    spe->is_loaded = true;
    spe->vaddr = vaddr;
    kpage->spe = spe;
    insert_spe(&(thread_current()->spt), kpage->spe);
    add_page_to_lru_list(kpage);

  return success;
}


/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  //printf("in install page\n");
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}



bool handle_mm_fault(struct spt_entry *spe){
   // page fault 핸들링을 위해서 호출되는 함수

   struct page* kpage;
   // page fault 발생시 물리페이지 할당
   // palloc_get_page() 함수 이용

   kpage = alloc_page_frame(PAL_USER);

   ASSERT(kpage != NULL);
   ASSERT(kpage->kaddr!=NULL);
   ASSERT(pg_ofs(kpage->kaddr)==0);
   ASSERT(spe!=NULL);
   

  if(spe->is_loaded == true){       // if vme is already loaded, return false
		free_page(kpage->kaddr);
		return false;
	}

   switch(spe->type){
      case VM_BIN:
      // Disk에 있는 file을 물리페이지(RAM)으로 load
      // 물리 메모리 적재 완료되면 가상주소와 물리주소 Page table로 맵핑
       if (!load_file (kpage->kaddr, spe))
          {
            NOT_REACHED ();
            free_page (kpage->kaddr);
            return false;
          }
          spe->is_loaded = true;
         break;
      case VM_FILE:
      // mmap 명령어를 통해 불려온 file을 물리페이지로 load
       if (!load_file (kpage->kaddr, spe))
          {
            NOT_REACHED ();
            free_page (kpage->kaddr);
            return false;
          }
          
         break;
      case VM_ANON:
      // 이미 swap_out된 메모리 영역을 다시 프로그램에서 호출시
      // swap_slot에 있는 정보를 다시 load
          swap_in(spe->swap_slot, kpage->kaddr);
          spe->is_loaded = true;
          //add_page_to_lru_list (kpage);
         break;
      default:
        printf("was it default?\n");
        return false;
   }

  // spe->vaddr 과 kpage->kaddr 에 대한 pagedir, pagetable mapping
  if(!install_page (spe->vaddr, kpage->kaddr, spe->writable))
  {
    free_page (kpage->kaddr);
    return false;
  }
  // set is_loaded status to true
  spe->is_loaded = true;
  kpage->spe = spe;
  // add to lru list
  add_page_to_lru_list(kpage);

   return true;
}
