#include "vm/swap.h"
#include "vm/frame.h"
#include <bitmap.h>
#include <debug.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

struct lock swap_lock;
struct bitmap *swap_map;
struct block *swap_block;

void swap_init(void)
{
	lock_init(&swap_lock);
	// get block
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block == NULL)
		return;
	// create bitmap
	swap_map = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE );
	if(swap_map == NULL)
		return;
	/// initialize bitmap to 0
	bitmap_set_all(swap_map, SWAP_FREE);
}

void swap_in(size_t used_index, void* kaddr)
{
	lock_acquire(&swap_lock);
	// check bitmap is empty slot
	if(bitmap_test(swap_map, used_index) == SWAP_FREE)
		return;
	// read the data from disk to memory
	for(int i=0; i<8; i++)
		block_read(swap_block, used_index * 8 + i, (uint8_t *)kaddr + i * BLOCK_SECTOR_SIZE);
	
	// filp to bitmap 1 to 0
	bitmap_flip(swap_map, used_index);
	lock_release(&swap_lock);
}

size_t swap_out(void *kaddr)
{
	size_t out_index;
	lock_acquire(&swap_lock);
	struct block *swap_block;
  	swap_block = block_get_role (BLOCK_SWAP);
	ASSERT(swap_map !=NULL);
	// find SWAP_FREE index. if there is no SWAP_FREE index, return
	out_index = bitmap_scan_and_flip(swap_map, 0, 1, SWAP_FREE);
	if(out_index == BITMAP_ERROR)
		return BITMAP_ERROR;
	// write to swap disk 
	for(int i=0; i<8; i++)
		block_write(swap_block, out_index * 8 + i, (uint8_t *)kaddr + i * BLOCK_SECTOR_SIZE);
	lock_release(&swap_lock);

	return out_index;
}