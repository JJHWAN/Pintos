#include "filesys/cache.h"
#include "filesys.h"
#include <string.h>

int cache_lru_clock = 0; // clock for lru algorithm
static struct buffer_cache_entry cache[NUM_CACHE]; // buffer cache array
static struct lock buffer_cache_lock; // lock for buffer read, write

// 핀토스 실행시 buffer_cache init
void buffer_cache_init(){
    // cache 배열에 대한 공간 할당
    // static struct buffer_cache_entry cache[NUM_CACHE];
    // lock 초기화
    for(int i =0;i<NUM_CACHE;i++){
        cache[i].valid_bit = 0;
        cache[i].dirty_bit = 0;
        cache[i].reference_bit = 0;
    }
    lock_init(&buffer_cache_lock);
    cache_lru_clock = 0;
}

// 종료시 호출?
void buffer_cache_terminate (){
    // 핀토스 종료시 호출
    // buffer_cache_flush_all을 호출해서
    // file persistency를 보장해주기 위해서 
    // buffer_cache 중 dirty bit가 1인 entry들을 모두 flush
    lock_acquire (&buffer_cache_lock);
    buffer_cache_flush_all();
    lock_release (&buffer_cache_lock);
}

// read 명령이 들어온 경우, 실행되는 함수
void buffer_cache_read (block_sector_t sector_idx, void* buffer){

    // cache 배열을 block_sector_t 로 검색해서
    // 1. 존재하는 경우, 그 영역을 읽는다. memcpy 함수를 통해 buffer에 cache buffer내의 block의 내용 복사
    // 2. 존재하지 않는 경우, LRU 알고리즘에 따라서 내부에 있던 cache를 flush하고 
    // 새로운 block_sector_t를 그 자리에 올리고 (with block_read()) 그 영역을 읽는다.
    // - 읽고 나서는 

    lock_acquire(&buffer_cache_lock);
    struct buffer_cache_entry* tmp = buffer_cache_lookup(sector_idx);

    if(tmp==NULL){
        tmp = buffer_cache_select_victim();
        tmp->disk_sector = sector_idx;
        block_read(fs_device, sector_idx, tmp->buffer);
        tmp->dirty_bit = 0;
        tmp->valid_bit = 1;
    }
    tmp->reference_bit = 1;
    // memcpy(buffer, tmp->buffer, BLOCK_SECTOR_SIZE);
    lock_release(&buffer_cache_lock);
    memcpy(buffer, tmp->buffer, BLOCK_SECTOR_SIZE);    
}
    

void buffer_cache_write (block_sector_t sector_idx, void* buffer){
    // buffer의 데이터를 buffer cache에 기록 (by memcpy())
    // buffer에 빈 영역이 없는 경우, victim entry 선정해서 디스크에 기록 ()

    lock_acquire(&buffer_cache_lock);
    struct buffer_cache_entry* tmp = buffer_cache_lookup(sector_idx);

    if(tmp==NULL){
        tmp = buffer_cache_select_victim();
        ASSERT(tmp!=NULL);
        tmp->disk_sector = sector_idx;
        block_read(fs_device, sector_idx, tmp->buffer);
        tmp->valid_bit = 1;
    }
    tmp->dirty_bit = 1;   
    tmp->reference_bit = 1;
    memcpy(tmp->buffer, buffer, BLOCK_SECTOR_SIZE);
    lock_release(&buffer_cache_lock);
}

struct buffer_cache_entry *buffer_cache_lookup (block_sector_t sector_idx){
    // sector_idx를 기준으로 cache 배열을 검색해서 있는 경우
    // 해당 포인터를 return
    // 없는 경우, NULL 을 return
    ASSERT(lock_held_by_current_thread(&buffer_cache_lock));

    for(int i =0;i<NUM_CACHE;i++){
        if(cache[i].valid_bit){
            if(cache[i].disk_sector == sector_idx){
                return &(cache[i]);
            }
        }
    }
    return NULL;
}
struct buffer_cache_entry *buffer_cache_select_victim (){
    // reference bit를 이용해서 reference bit가 1인 경우, 0으로 내리고 다음 배열로 이동
    // cache_lru_clock 이 NULL인 경우, cache[0]부터 시작
    ASSERT(lock_held_by_current_thread(&buffer_cache_lock));

    while(1){
        if(cache_lru_clock == NUM_CACHE-1){
            cache_lru_clock = 0;
        }
        if(cache[cache_lru_clock].reference_bit == 1){
            cache[cache_lru_clock].reference_bit = 0;
        }
        else{
            // found victim
            buffer_cache_flush_entry(&cache[cache_lru_clock]);
            cache[cache_lru_clock].reference_bit = 0;
            cache[cache_lru_clock].dirty_bit = 0;
            cache[cache_lru_clock].valid_bit = 0;
            return &(cache[cache_lru_clock]);
        }
        cache_lru_clock++;
    }

    // LRU 알고리즘을 통해 배당 buffer의 내용을 flush하고 
    // dirty bit = 0, reference bit = 0
    // victim을 return

}
void buffer_cache_flush_entry(struct buffer_cache_entry* bc){
    // bc의 buffer[BLOCK_SECTOR_SIZE]에 담긴 내용을 
    // bc의 disk_sector에 해당하는 부분에 write
    // block_write 함수 이용
    ASSERT(lock_held_by_current_thread(&buffer_cache_lock));

    if(bc->dirty_bit){
        block_write(fs_device, bc->disk_sector, bc->buffer);
    }
}

void buffer_cache_flush_all() {

    for(int i=0;i<NUM_CACHE;i++){
        if(cache[i].valid_bit){
            buffer_cache_flush_entry(&cache[i]);
        }
    }
   
}