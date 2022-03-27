#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"

extern struct lock filesys_lock;
struct spt_entry *find_spe(void *vaddr);

// hash table 초기화
void spt_init(struct hash* spt){
    ASSERT(spt!=NULL);
    hash_init(spt, spt_hash_func, spt_less_func, NULL);
}

// hash_entry()로 hash table 내에 구조체 검색
// hash_int()을 이용해서 해시값을 구하고 반환
unsigned spt_hash_func(const struct hash_elem *e, void *aux UNUSED){
    ASSERT (e != NULL);
    return hash_int ((int)hash_entry (e, struct spt_entry, elem)->vaddr);
}
bool spt_less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux){
    void* A = hash_entry(a, struct spt_entry, elem)->vaddr;
    void* B = hash_entry(b, struct spt_entry, elem)->vaddr;
    return A < B;
}

bool insert_spe(struct hash *spt, struct spt_entry *spe){
    ASSERT (spt != NULL);
    ASSERT (spe != NULL);
    ASSERT (pg_ofs (spe->vaddr) == 0);
    //printf("insert spe->vaddr : %X\n", spe->vaddr);
    return hash_insert (spt, &spe->elem) == NULL;
}

bool delete_spe(struct hash *spt, struct spt_entry *spe){
    ASSERT (spt != NULL);
    ASSERT (spe != NULL);
    if (!hash_delete (spt, &spe->elem))
        return false;
    //free_page_vaddr (spe->virtual_addr);
    //swap_clear (vme->swap_slot);
    free (spe);
    return true;
}

struct spt_entry *find_spe(void *vaddr){
    
    struct hash *spt;
    struct spt_entry *spe = (struct spt_entry*)malloc(sizeof(struct spt_entry));
    struct hash_elem *elem;
    void *temp = vaddr;
    
    spt = &(thread_current ()->spt);
    spe->vaddr = pg_round_down (vaddr);
    ASSERT (pg_ofs (spe->vaddr) == 0);
    elem = hash_find (spt, &spe->elem);
    free(spe);
    return elem ? hash_entry (elem, struct spt_entry, elem) : NULL;
}

void spt_destroy(struct hash *spt){
    ASSERT(spt!=NULL);
    hash_destroy(spt, spt_destroy_func);
}

void spt_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  ASSERT (e != NULL);
  struct spt_entry *spe = hash_entry (e, struct spt_entry, elem);
  void *kaddr;

  if(spe->is_loaded){
      kaddr = pagedir_get_page(thread_current()->pagedir, spe->vaddr);
      free_page(kaddr);
      pagedir_clear_page(thread_current()->pagedir, spe->vaddr);
  }
  //swap_clear (spe->swap_slot);
  free (spe);
}

//----------------------For Demanding Paging---------------------

// Disk에 존재하는 page를 물리메모리(RAM)으로 load하는 함수
bool load_file(void* kaddr, struct spt_entry *spe){
    // use spe <file, offset> to read a page to kaddr 
    // use file_read_at() or file_read() & file_seek()

    ASSERT (kaddr != NULL);
    ASSERT (spe != NULL);
    ASSERT (spe->type == VM_BIN || spe->type == VM_FILE);
   
    int a = file_read_at (spe->file, kaddr, spe->read_bytes, spe->offset);
    if (a != (int) spe->read_bytes)
    {
      return false;
    }
    //printf("load_file sucess!\n");
    memset (kaddr + spe->read_bytes, 0, spe->zero_bytes);
  return true;
}