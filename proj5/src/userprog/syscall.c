#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <stdlib.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <list.h>
#include "threads/malloc.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

struct lock filesys_lock;

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };


static void syscall_handler (struct intr_frame *);
void rm_spt_umap(struct mmap_file* mmap_file);
void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
void check_user_address(void* addr){

  //printf("addr: %X\n", (unsigned int)addr);
  if(!is_user_vaddr(addr)){
    //printf("user addr fault : addr(%d)\n", (int)addr);
    exit(-1);
  }
}
*/

void check_kernel_address(void* addr){
   if(!is_kernel_vaddr(addr)){
    printf("kernel addr fault : addr(%d)\n", (int)addr);
    exit(-1);
  }

}

struct spt_entry* check_user_address(void* addr){
  if(addr < (void*)0x08048000 || addr >= (void*)0xc0000000){
    //printf("check_user_address : addr : %X\n", addr);
    exit(-1);
  }
  // addr??spt_entry??議댁옱?섎㈃, spt_entry瑜?諛섑솚?섎룄濡?肄붾뱶 ?묒꽦 
  return find_spe(addr);
}

void check_valid_buffer(const char *buffer, unsigned size, bool to_write){
  ASSERT(buffer!=NULL);

  unsigned i;
  struct spt_entry* spe;
  for(i=0;i<size;i++){
    if((spe = check_user_address((void*)buffer+i)) == NULL){
      if(to_write){
        if(!(spe->writable)){
          //printf("check valid buffer not writable\n");
            exit(-1);
        }
      }
    }
  }
}

void check_valid_string(const void *str)
{
	char *check_str = (char *)str;
	check_user_address((void *)check_str);
	/* check the all string's address */
	while(*check_str != 0)
	{
		check_str += 1;
		check_user_address(check_str);
  }
}


// ?꾩옱 pintos??system call handler媛 援ы쁽?섏뼱 ?덉? ?딆븘 ?쒖뒪??肄쒖씠 ?몄텧?????놁뼱
// user program???뺤긽?곸쑝濡??숈옉?섏? ?딅뒗 ?곹깭?대떎.

// System call?대? user program???묐룞?섎뒗???덉뼱 而ㅻ꼸 湲곕뒫???ъ슜?????덈룄濡??댁쁺 泥댁젣媛 ?쒓났?섎뒗 ?명꽣?섏씠?ㅼ씠??
// ?덈? ?ㅼ뼱, ?뱀젙 user program???묐룞?섎㈃??硫붾え由??쎄린 諛??곌린? 媛숈? 而ㅻ꼸 湲곕뒫?????request瑜?蹂대궡硫? 
// 而ㅻ꼸 ?곸뿭?먯꽌 ?쒖뒪??肄쒖씠 ?ㅽ뻾?섏뼱 泥섎━ ??寃곌낵 媛믪쓣 ?섍꺼二쇰뒗 ?앹엯?덈떎. 
// 利? ?꾨줈?앺듃1?먯꽌 ?몃? timer 諛?I/O device濡쒕??곗쓽 interrupt瑜?泥섎━?섎뒗 寃껉낵 媛숈씠, 
// ?뚰봽?몄썾???대??먯꽌 諛쒖깮?섎뒗 interrupt 諛?exception瑜?泥섎━?섍린 ?꾪븳 湲곕뒫??寃껋엯?덈떎.

// fibonacci, max_of_four_int 異붽? 援ы쁽 ?꾩슂
// read, write??寃쎌슦 special case..?
// read, write媛 STDIN/ STDOUT 留뚯쓣 媛?뺥븯怨?留뚮뱶??寃껋쓣 ?섎??섎뒗??

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  //printf("system num : %d\n", *(uint32_t *)(f->esp));
  int i;

  check_user_address(f->esp);

  //hex_dump((uint32_t)(f->esp), (f->esp), 100, 1);

  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_user_address(f->esp+4);
      exit((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_EXEC:
      check_user_address(f->esp+4);
      check_valid_string(f->esp+4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_WAIT:
      //hex_dump(f->esp, f->esp, 100, 1);
      check_user_address(f->esp+4);
      f->eax = wait((pid_t)*(uint32_t *)(f->esp+4));
      break;
    case SYS_CREATE:
      // syscall2, return bool
      for(i=1;i<=2;i++)
        check_user_address(f->esp+i*4);
      check_valid_string((f->esp+4));
      f->eax = create((const char *)*(uint32_t *)(f->esp+4), (unsigned)(*(uint32_t *)(f->esp+8)));
      break;
    case SYS_REMOVE:
      // syscall1
      check_user_address(f->esp+4);
      check_valid_string((f->esp+4));
      f->eax = remove((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_OPEN:
      // syscall1
      check_user_address(f->esp+4);
      check_valid_string((f->esp+4));
      f->eax = open((const char *)*(uint32_t *)(f->esp+4));   
      break;
    case SYS_FILESIZE:
      // syscall1
      check_user_address(f->esp+4);
      f->eax = filesize((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_READ:
      for(i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      f->eax = read((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_WRITE:
    // 二쇱냼???ㅼ뼱?덈뒗 媛믪쓣 媛?몄삤硫??쒕떎!! ?댁씪 ?닿껐
      for(i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      check_valid_string((f->esp+4));
      f->eax = write((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_FIBONACCI:
      check_user_address(f->esp+4);
      f->eax = fibonacci((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_MAX_OF_FOUR:
      for(i=1;i<=4;i++)
        check_user_address(f->esp+i*4);
      f->eax = max_of_four_int((int)(*(uint32_t *)(f->esp+4)),  (int)(*(uint32_t *)(f->esp+8)), (int)(*(uint32_t *)(f->esp+12)), (int)(*(uint32_t *)(f->esp+16)));
      break;
    case SYS_SEEK:
      // syscall2
      for(i=1;i<=2;i++)
        check_user_address(f->esp+i*4);
      seek((int)(*(uint32_t *)(f->esp+4)), (unsigned)(*(uint32_t *)(f->esp+8)));
      break;
    case SYS_TELL:
      // syscall1
      check_user_address(f->esp+4);
      f->eax = tell((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_CLOSE:
      // syscall1
      check_user_address(f->esp+4);
      close((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_MMAP:
      for(i=1;i<=2;i++)
        check_user_address(f->esp+i*4);
      f->eax = mmap((int)(*(uint32_t *)(f->esp+4)), (void*)(*(uint32_t *)(f->esp+8)));
      break;
    case SYS_MUNMAP:
      check_user_address(f->esp+4);
      munmap((int)(*(uint32_t *)(f->esp+4)));
      break;
      case SYS_ISDIR:
      check_user_address(f->esp+4);
      f->eax = sys_isdir((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_CHDIR:
      check_user_address(f->esp+4);
      f->eax = sys_chdir((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_MKDIR:
      check_user_address(f->esp+4);
      f->eax = sys_mkdir((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_READDIR:
      check_user_address(f->esp+4);
      check_user_address(f->esp+8);
      f->eax = sys_readdir((int)(*(uint32_t *)(f->esp+4)), (char*)(*(uint32_t *)(f->esp+8)));
      break;
    case SYS_INUMBER:
      check_user_address(f->esp+4);
      f->eax = sys_inumber((int)(*(uint32_t *)(f->esp+4)));
      break;
  }
}

// return ondisk block sector
int sys_inumber(int fd){
  struct file* fptr = thread_current()->fd[fd];
  struct inode* inode;
  if(fptr != NULL){
    inode = file_get_inode(fptr);
    //printf("fd (%d) : inode inumber %d\n", fd, inode_get_inumber(inode));
    return inode_get_inumber(inode);
  }
  // file does not exist
  return -1;
}

bool sys_readdir(int fd, char* name){

  struct file* fptr = thread_current()->fd[fd];
  struct inode* inode = file_get_inode(fptr);
  struct dir* dir;
  int offset = 0;
  char entry[20] ={'\0'};
  bool result = false;
  char *cp_name;

  lock_acquire(&filesys_lock); 

  if(thread_current()->dir[fd]==NULL)
    thread_current()->dir[fd] = dir_open(file_get_inode(fptr));

  if(fptr!=NULL){
    // if file is not a directory return false;
    if(!inode_is_directory(inode)){
      return false;
    }

    dir = thread_current()->dir[fd];
    
    // read directory and store name

    //printf("in readdir fd(%d) \n ", fd);
    //check_dir_entry(dir);
    
    while(dir_readdir(dir, entry) == true)
		{
      //printf("dir pos : %d\n", dir_get_pos(dir));
      /* read directory except . and .. */
			if( strcmp(entry,".") == 0 || strcmp(entry,"..") == 0 ){
        continue;
      }  	
			/* copy entry to name */
      //printf("passed entry : %s, len %d\n", entry, strlen(entry));
			strlcpy(name, entry, strlen(entry)+1);
			offset = strlen(entry) + 1;
      result = true;
      break;
		}

    if(strlen(entry)==0){
      result = false;
    }
	}

  //printf("result send %d\n", result);
  lock_release(&filesys_lock); 

	return result;
} 

bool sys_mkdir(const char *dir){
  //lock_acquire(&filesys_lock); 

  bool result =  filesys_create_dir(dir);
  //lock_release(&filesys_lock); 

  return result;
}


// change working directory
bool sys_chdir(const char* dir){

  // dir 경로를 분석해서 directory 반환
  bool result = false;
  //lock_acquire(&filesys_lock); 
  struct file* directory = filesys_open(dir);
  if(directory != NULL){
    // close current thread's directory
    dir_close(thread_current()->t_dir);
    // 스레드의 현재 작업 디렉토리를 변경
    thread_current()->t_dir = dir_open(file_get_inode(directory));
    result = true;
  }
  //lock_release(&filesys_lock); 

  return result;
}

bool sys_isdir(int fd){
  
  bool result;
  //lock_acquire(&filesys_lock); 

  struct file* fptr = thread_current()->fd[fd];
  result = inode_is_directory(fptr->inode);
  //lock_release(&filesys_lock); 

  return result; 
}
// What wait() system call should do is "wait child process until it finishes its work."
// - Check that child thread ID is valid
// - Get the exit status from child thread when the child thread is dead
// - To prevent termination of process before return from wait(), you can use busy waiting technique* or thread_yield() in threads/thread.c.


int wait(pid_t pid){
  // Waits for a child process pid and retrieves the child's exit status.
  return process_wait(pid);
}


void exit(int status){

  struct thread *cur = thread_current();
  if(lock_held_by_current_thread (&filesys_lock)){
      lock_release(&filesys_lock);
  }
  printf("%s: exit(%d)\n", thread_name(), status);
  cur -> exit_status = status;
  for(int i=3;i<128;i++){
    if(cur->fd[i]!=NULL){
      close(i);
    }
  }
  thread_exit();
}


int write (int fd, const void *buffer, unsigned size){
  // writes 'size' bytes from 'buffer' to the open file 'fd'
  // use 'putbuf()'

  check_valid_buffer(buffer, size, 0);

  struct file* fptr = thread_current()->fd[fd];
  int return_val = -1;
  lock_acquire(&filesys_lock);
  if(fd==1){
    putbuf(buffer, size);
    lock_release(&filesys_lock);
    return size;
  }
  else if (fd>2) {
    if(fptr==NULL){
      lock_release(&filesys_lock);
      exit(-1);
    }
    
    //  printf("Wrote a (%d)\t", thread_current()->tid);
    return_val = file_write(thread_current()->fd[fd], buffer, size);
    //  printf("Wrote R (%d)\n", thread_current()->tid);
    //lock_release(&filesys_lock);
  }
  lock_release(&filesys_lock); 
  return return_val;
}

int read (int fd, void* buffer, unsigned size){

  // returns actually read bytes or -1 when can't be read.
  // fd 0 reads from the keyboard using input_getc()
  // esp+4 | esp+8 | esp+12 ?대젃寃??쏀엺??

  unsigned i = -1;
  int k = 0;
  if(buffer==NULL)
    exit(-1);

  check_valid_buffer(buffer, size, 1);


  //lock_acquire(&filesys_lock);
  // printf("Read a(%d)\t", thread_current()->tid);
  if (fd==0){
    for (i = 0; i < size; i++, k++)
	  {
	    ((char*)buffer)[i] = input_getc();
      if(((char*)buffer)[i] =='\0')
        break;
	  }
  }
  else if (fd>2){
    if(thread_current()->fd[fd]==NULL){ 
      //printf("Read R (%d)\n", thread_current()->tid);
      //lock_release(&filesys_lock);
      return -1;
    }
    lock_acquire(&filesys_lock);
    k = file_read(thread_current()->fd[fd], buffer, size);
    lock_release(&filesys_lock);
    //printf("tid(%d) : f->pos %d\n", thread_current()->tid, thread_current()->fd[fd]->pos);
  }
  //  printf("Read R (%d)\n", thread_current()->tid);

  //lock_release(&filesys_lock);
  return k;
}

void halt(){
  // terminates pintos
  shutdown_power_off();
}

pid_t exec(const char *cmd_line){
  // runs the executable whos name is given in cmd_line,
  // passing any given arguments,
  // returns the new process's programs id(pid)
  // if program can't be load or run for any reason, must return -1

  //printf("In exec : cmd_line : %s\n", cmd_line);
  //for(int i=0;i<100;i++){
  //  printf("exec:\n");
  //}
  lock_acquire(&filesys_lock);
  pid_t pid = process_execute(cmd_line);
  lock_release(&filesys_lock);
  return pid;
}

int fibonacci(int n){

  int *array, result, i;

  if(n<2)
    return 1;
  
  array = (int*)malloc(sizeof(int)*(n+1));
  array[0] = array[1] = 1;
  for(i=2;i<n;i++){
    array[i] = array[i-1] + array[i-2];
    //printf("array[%d]: %d\n", i, array[i]);
  }
  
  result = *(array+n-1);
  free(array);

  return result;
}

int max_of_four_int(int a, int b, int c, int d){

  int n1, n2, result;

  n1 = a>b ? a : b;
  n2 = c>d ? c : d;
  result = n1>n2 ? n1 : n2;

  return result;
}

// ----------------Proj2-------------------------

bool create (const char *file, unsigned initial_size){
  // ?덈줈???뚯씪 ?앹꽦 initial_size byte 留뚰겮???ш린瑜?媛吏? 
  // ?깃났?곸쑝濡??앹꽦??true return , else false return
  // filesys/filesys.c???덈뒗 filesys_create ?⑥닔瑜??댁슜

  if(file==NULL){
    exit(-1);
  }
  lock_acquire(&filesys_lock); 
  //printf("Created a (%d)\t", thread_current()->tid);
  bool result = filesys_create(file, initial_size);
  //printf("Created R (%d)\n", thread_current()->tid);
  lock_release(&filesys_lock);

  //printf("file name : %s\n", file);
  return result;
}

bool remove (const char *file){
  if(file==NULL){
    return false;
  }

  lock_acquire(&filesys_lock); 
  bool result = filesys_remove(file);
  lock_release(&filesys_lock);
  return result;

  //return filesys_remove(file);
}

int open (const char *file){
  // when single file is opened more than once,
  // each open returns a new fd.
  // Those different fd is closed in independent call of close()

  if(file==NULL){
    //printf("here? file NULL\n");
    return -1;
  }
  lock_acquire(&filesys_lock);
  //printf("Opend a(%d)\t", thread_current()->tid);
  struct file* f = filesys_open(file);
 // printf("Created a\t");
  //lock_release(&filesys_lock);
  struct thread* t = thread_current();
  
  if(f==NULL){
    //printf("here? f NULL\n");
    lock_release(&filesys_lock);
    return -1;
  }

  // i=0,1,2??STD?쇰줈 ?뺥빐?몄엳??
  struct inode* inode;
  int i, flag = 0;
  for(i=3;i<128;i++){
    if(t->fd[i]==NULL){
      if (strcmp(thread_current()->name, file) == 0) {
          file_deny_write(f);
      }   
      //printf("tid (%d) : filename:%s inumber : %d\n", t->tid, file, inode_get_inumber(file_get_inode(f)));
      t->fd[i] = f; 
      //printf("tid(%d) file_inode_length : %d, file_pos : %d\n", t->tid, inode_length(file_get_inode(t->fd[i])), f->pos);
      lock_release(&filesys_lock);
      return i;
    }
  }
  lock_release(&filesys_lock);
  return -1;
}

void close (int fd){

  if(thread_current()->fd[fd]==NULL){
    exit(-1);
  }
  lock_acquire(&filesys_lock);
  
  if(thread_current()->dir[fd]!=NULL) 
    dir_close(thread_current()->dir[fd]);
  //free(thread_current()->dir[fd]);
  thread_current()->dir[fd] = NULL;
  file_close(thread_current()->fd[fd]);
  //printf("close fd done\n");
  // int temp = 0;
  //   for(long long int i =0;i<100;i++){
  //     printf("1");
  //   }
  thread_current()->fd[fd] = NULL;
  lock_release(&filesys_lock);
}

int filesize (int fd){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  return file_length(f);
}

void seek (int fd, unsigned position){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  file_seek(f, position);
}

unsigned tell (int fd){
  struct file* f = thread_current()->fd[fd];
  if(f==NULL)
    exit(-1);
  return file_tell(f);
}

// ---------------- Proj4 ----------------------

int mmap(int fd, void* addr){

  struct mmap_file *mmap_file;
  size_t offset = 0;

  if (pg_ofs (addr) != 0 || !addr)
    return -1;
  if (is_user_vaddr (addr) == false)
    return -1;
  mmap_file = (struct mmap_file *)malloc (sizeof (struct mmap_file));
  if (mmap_file == NULL)
    return -1;
  memset (mmap_file, 0, sizeof(struct mmap_file));
  list_init (&mmap_file->spe_list);
  if (!(mmap_file->file = thread_current()->fd[fd]))
    return -1;
  mmap_file->file = file_reopen(mmap_file->file);
  mmap_file->mapid = thread_current ()->next_mapid++;
  list_push_back (&thread_current ()->mmap_list, &mmap_file->elem);

  int length = file_length (mmap_file->file);
  while (length > 0)
    {
      if (find_spe (addr))
        return -1;

      struct spt_entry *spe = (struct spt_entry *)malloc (sizeof (struct spt_entry));
      memset (spe, 0, sizeof (struct spt_entry));
      spe->type = VM_FILE;
      spe->writable = true;
      spe->vaddr = addr;
      spe->offset = offset;
      spe->read_bytes = length < PGSIZE ? length : PGSIZE;
      spe->zero_bytes = PGSIZE - spe->read_bytes;
      spe->file = mmap_file->file;
      insert_spe (&thread_current ()->spt, spe);
      list_push_back (&mmap_file->spe_list, &spe->mmap_elem);
      addr += PGSIZE;
      offset += PGSIZE;
      length -= PGSIZE;
    }
  return mmap_file->mapid;
}

void rm_spt_umap(struct mmap_file* mmap_file){
    struct thread *t = thread_current();
    struct list_elem *elem, *temp;
    struct list *spe_list = &(mmap_file->spe_list);
    struct spt_entry *spe;
    void* kaddr;

    elem = list_begin(spe_list);

    for(; elem != list_end(spe_list); elem = list_next(elem)){
      spe = list_entry(elem, struct spt_entry, mmap_elem);
      //printf("in for?\n");
      if(spe->is_loaded==true){
        kaddr = pagedir_get_page(t->pagedir, spe->vaddr);
        // if dirty bit true, write to disk
        if(pagedir_is_dirty(t->pagedir, spe->vaddr)==true){
          lock_acquire(&filesys_lock);
          file_write_at(spe->file, spe->vaddr, spe->read_bytes, spe->offset);
          lock_release(&filesys_lock);
          spe->is_loaded = false;
        }
        // clear page table
        pagedir_clear_page(t->pagedir, spe->vaddr);
        //printf("before free page?\n");
        free_page(kaddr);
        //printf("after free page?\n");
      }
      temp = list_prev(elem);
      list_remove(elem);
      elem = temp;
      delete_spe(&t->spt, spe);
    }
}

void munmap(int mapid){

  struct mmap_file *mmap_file;
  struct thread* t = thread_current();

  struct list_elem* elem,  *temp;
  for(elem = list_begin(&t->mmap_list) ; elem != list_end(&t->mmap_list) ; elem = list_next(elem)){
    mmap_file = list_entry(elem, struct mmap_file, elem);
    if(mapid==-1){
      rm_spt_umap(mmap_file);
      file_close(mmap_file->file);
      temp = list_prev(elem);
      list_remove(elem);
      elem = temp;
      free(mmap_file);
      continue;
    }
    else if(mapid == mmap_file->mapid){
      rm_spt_umap(mmap_file);
      lock_acquire(&filesys_lock);
      file_close(mmap_file->file);
      lock_release(&filesys_lock);
      temp = list_prev(elem);
      list_remove(elem);
      elem = temp;
      free(mmap_file); 
      break;
    }
  }

}