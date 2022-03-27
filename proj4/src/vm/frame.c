#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/frame.h"
//#include "userprog/syscall.c"

extern struct lock filesys_lock;
/* lock to synchronize between processes on frame table */
struct lock vm_lock;

bool flag;
int cnt = 0;

void lru_list_init(){
  list_init(&lru_list);
  lock_init(&vm_lock);    
  lru_clock = NULL;
}

// allocate a page from USER POOL with palloc_get_page
// add an entry to frame table if succed
struct page* alloc_page_frame(enum palloc_flags flags){

    struct page* frame;
    void* kaddr;
    frame = (struct page*)malloc(sizeof(struct page));
    memset(frame, 0,sizeof(struct page));
    frame->t = thread_current();
    kaddr = palloc_get_page (flags);
 
    if (kaddr==NULL){
        try_to_free_pages();
        kaddr = palloc_get_page(flags);
    }

    //printf("kaddr : %p\n", kaddr);
    frame->kaddr = kaddr;
    //add_page_to_lru_list(frame);

    return frame;
}


/* Add an entry to frame table */
void add_page_to_lru_list(struct page* page)
{
  
  ASSERT (page);
  ASSERT (pg_ofs (page->kaddr) == 0);
  lock_acquire (&vm_lock);
  list_push_back (&lru_list, &page->lru);
  lock_release (&vm_lock);
}


/* Remove the entry from frame table and free the memory space */
void delete_from_lru_list (struct page *page)
{
  ASSERT (page);
  //printf("deleting page : %p\n", page->spe->vaddr);

  lock_acquire(&vm_lock);
  if (lru_clock == &page->lru)
    {
      lru_clock = list_remove (lru_clock);
    }
  else
    {
      list_remove (&page->lru);
    }
  lock_release(&vm_lock);
}

// get page with kaddr
struct page* get_page_with_kaddr (void *kaddr)
{

  ASSERT (pg_ofs (kaddr) == 0);
  struct page* temp;
  struct list_elem *e;
  
  lock_acquire (&vm_lock);
  e = list_head (&lru_list);
  while ((e = list_next (e)) != list_tail (&lru_list))
    {
      temp = list_entry (e, struct page, lru);
      if (temp->kaddr == kaddr)
        break;
      temp = NULL;
    }
  lock_release (&vm_lock);

  return temp;
}

// kaddr을 기반으로 page free
void free_page(void* kaddr){
    struct page* page = get_page_with_kaddr(kaddr);
    ASSERT(page!=NULL);
    __free_page (page);

}

// free_page의 실제 실행 파트
void __free_page (struct page* page){
    delete_from_lru_list(page);
    palloc_free_page(page->kaddr);
    free(page);
}

void print_all_lru(void){

  struct page* temp;
  struct list_elem *e;

  e = list_head (&lru_list);
  printf("PRINT ALL\n\n\n");
  while ((e = list_next (e)) != list_tail (&lru_list))
    {
      temp = list_entry (e, struct page, lru);
      printf("page->kaddr : %p\n", temp->spe->vaddr);
    }
}


static struct list_elem* get_next_lru_clock(void){

  ASSERT(!list_empty(&lru_list));

  if(lru_clock==NULL){
      lru_clock = list_begin(&lru_list);
  }
  else lru_clock = list_next(lru_clock);
  
  if(lru_clock == list_tail(&lru_list))
    lru_clock = list_begin(&lru_list);
  return lru_clock;
}

void try_to_free_pages (void){

  ASSERT(!list_empty(&lru_list));

  struct page *page, *temp;
  struct list_elem* elem = NULL;
  struct thread* t;

  lock_acquire (&vm_lock);

  //printf("in try to free pages\n");

  while(1){
    
    elem = get_next_lru_clock();
    //if(elem == list_end(&lru_list)) printf("end!\n");
    page = list_entry(elem, struct page, lru);
    t = page->t;

     if (pagedir_is_accessed(t->pagedir, page->spe->vaddr)){
        pagedir_set_accessed(t->pagedir, page->spe->vaddr, false);
        continue;
    }  
    // else it is a victim
    if (pagedir_is_dirty(t->pagedir, page->spe->vaddr) || page->spe->type == VM_ANON){
      // if spe is mmap file, don't call swap out
      if(page->spe->type == VM_FILE)
			{
        lock_acquire(&filesys_lock);
				file_write_at(page->spe->file, page->kaddr ,page->spe->read_bytes, page->spe->offset);
        lock_release(&filesys_lock);
      }
      else{
        page->spe->type = VM_ANON;
        page->spe->swap_slot = swap_out(page->kaddr);
      }

      page->spe->is_loaded = false;
		  pagedir_clear_page(t->pagedir, page->spe->vaddr);
      
      lock_release(&vm_lock);
		  
		  break;
    }
  }

  __free_page(page);
  //printf("how did you get here?\n");
  //lock_release (&vm_lock);
  
  return ;
}