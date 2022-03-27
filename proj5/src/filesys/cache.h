#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#define NUM_CACHE 64

struct buffer_cache_entry {
    bool valid_bit; // 값이 제대로 들어와 있는 경우, set
    bool reference_bit; // for clock algorithm
    bool dirty_bit; // 내부 값의 변화가 있는 경우

    block_sector_t disk_sector; // 해당 entry가 가르키는 disk의 sector
    uint8_t buffer[BLOCK_SECTOR_SIZE]; // 512*18
};

void buffer_cache_init(void);

// 종료시 호출?
void buffer_cache_terminate (void);


void buffer_cache_read (block_sector_t sector_idx, void* buffer);
void buffer_cache_write (block_sector_t sector_idx, void* buffer);
struct buffer_cache_entry *buffer_cache_lookup (block_sector_t);
struct buffer_cache_entry *buffer_cache_select_victim (void);
void buffer_cache_flush_entry(struct buffer_cache_entry*);
void buffer_cache_flush_all(void);
