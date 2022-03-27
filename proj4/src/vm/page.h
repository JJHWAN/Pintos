#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include "lib/kernel/hash.h"
#include "filesys/file.h"
#include <list.h>
#include "threads/vaddr.h"

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2


struct mmap_file{ // mmap syscall 을 위한 구조체
    int mapid; // 이 file의 mapid (식별)
    struct file* file; // 연결된 file
    struct list_elem elem; // list 순회, 검색을 위한 elem
    struct list spe_list; // mmap에 연관된 spe를 관리하기 위한 list
};

struct page{
    void *kaddr; // 이 page에 연결된 physical address
    struct spt_entry *spe; // 이 page에 연결된 vaddr를 지니는 spt_entry
    struct thread *t; // 이 page에 연결된 thread
    struct list_elem lru; // lru_list 검색을 위한 list_elem
};

struct spt_entry{

    uint8_t type; // VM_BIN, VM_FILE, VM_ANON의 타입
    void *vaddr; // vm_entry가 관리하는 가상페이지의 번호

    bool writable; // true일 경우 해당 주소에 write 가능
    bool is_loaded; // 물리 메모리의 탑재 여부를 알려주는 플래그

    struct file* file; // 가상주소와 맵핑된 파일

    size_t offset;     // 읽어야 할 파일의 offset
    size_t read_bytes; // 가상페이지에 쓰여있는 데이터의 크기
    size_t zero_bytes; // 0으로 채울 남은 페이지의 바이트
    
    struct hash_elem elem; // thread의 hash spt에 들어갈 hash_elem
    struct list_elem mmap_elem; // for mmap_list

    size_t swap_slot; // for swapping
};

// hash table 초기화
void spt_init(struct hash* spt);
// hash_entry()로 hash table 내에 구조체 검색
// hash_int()을 이용해서 해시값을 구하고 반환
unsigned spt_hash_func(const struct hash_elem *e, void *aux);
// hash init을 위한 함수
bool spt_less_func(const struct hash_elem *a, const struct hash_elem *b, void* aux);
// insert spe to spt
bool insert_spe(struct hash *spt, struct spt_entry *spe);
// delte spe from spt
bool delete_spe(struct hash *spt, struct spt_entry *spe);
// destroy spt using hash_destroy
void spt_destroy(struct hash *spt);
// func for spt_destory
void spt_destroy_func (struct hash_elem *e, void *aux UNUSED);
// find spe with vaddr and return spe
struct spt_entry* find_spe(void* vaddr);

//----------------------For Demanding Paging---------------------
// Disk에 존재하는 page를 물리메모리(RAM)으로 load하는 함수
// by using file_read_at(), and memset()
bool load_file(void *kaddr, struct spt_entry* spe);

#endif