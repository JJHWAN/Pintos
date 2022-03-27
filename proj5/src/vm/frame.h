#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include <list.h>
#include "vm/page.h"

/* frame_list is being used for storing the FIFO information 
required for the second chance eviction algorithm. */

// LRU 알고리즘을 적용하기 위해 page들을 저장하고 있는 list
struct list lru_list;
// LRU 알고리즘의 포인터 
struct list_elem* lru_clock;

// init lru_list, lru_clock, vm_lock
void lru_list_init(void);

// palloc_page 함수를 대체하는 alloc_page_frame
// page에 정보를 저장하고, 메모리에 공간이 없는 경우
// try_to_free_page를 호출해서 공간을 만들고 그 공간을 할당한다.
struct page* alloc_page_frame (enum palloc_flags flags);

// push back page to lru_list
void add_page_to_lru_list(struct page* page);
// delelte page from lru_list
void delete_from_lru_list(struct page* page);

// free_page and pagedir with ti
void free_page(void* kaddr);
void __free_page (struct page* page);

// get page structure by kaddr
struct page* get_page_with_kaddr (void *kaddr);

// lru list의 다음 노드를 return 
static struct list_elem* get_next_lru_clock(void);

// when page is full called
// LRU 알고리즘에 근거해서 page를 free해준다.

// get_next_lru() 함수를 이용해 page들을 순회하고,
// pagedir_is_accessed()함수를 이용해 해당 vaddr에 대해 access bit를 검사한다.
// 만약 access bit가 0이라면 pagedir_set_accessed() 함수를 이용해서 1로 set하고 continue;
// 만약 1이라면, dirty bit를 pagedir_is_dirty() 함수를 이용해 검사하고,
// dirty bit 1인 경우거나, page->spe->type = VM_ANON인 경우, swap_out 혹은 file_write_at를 해주고
// free 한다.
void try_to_free_pages (void);

#endif