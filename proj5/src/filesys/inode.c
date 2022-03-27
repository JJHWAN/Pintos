#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/synch.h"
#include <stdlib.h>
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

static inline size_t min (size_t a, size_t b)
{
  if(a<b)
    return a;
  else 
    return b;
}

// -------------Proj5--------------------
// inode에 direct 방식으로 저장할 블록의 수
#define DIRECT_BLOCKS_COUNT 123
// 하나의 index 블록이 저장할 수 있는 인덱스 블록 (512/4)
#define INDIRECT_BLOCKS_PER_SECTOR 128

struct inode_disk
  {
    off_t length;  /* File size in bytes. */
    unsigned magic; /* Magic number. */
    bool isdir; // flag for dir or file
    // for direct mapping (123)
    block_sector_t direct_map_table[DIRECT_BLOCKS_COUNT];
    // for indirect mapping (128)
    block_sector_t indirect_block; 
    // for double indirect mapping (128*128)
    block_sector_t double_indirect_block;
};

struct inode 
{
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;              /*Inode content. */
};

// to keep track of indirect block
struct inode_indirect_block_sector {
  // indirect block 관리를 위한 structure
  block_sector_t blocks[INDIRECT_BLOCKS_PER_SECTOR];
};

// -------------------------- Proj5
// return inode's open_cnt
int inode_open_cnt(struct inode* inode){
  return inode->open_cnt;
}
// return inode->removed
bool inode_removed_or_not(struct inode *inode){
  return inode->removed;
}
// return inode->sector
block_sector_t inode_sector_num(struct inode* inode){
  return inode->sector;
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

// return inode is dir or not
bool inode_is_directory (const struct inode *inode)
{
  return inode->data.isdir;
}

// by index find the sector in disk
// useful for read and write
static block_sector_t index_to_sector(const struct inode_disk* inode_disk, off_t index){

  off_t start = 0, limit = 0;
  block_sector_t temp;

  // 1. direct
  limit += DIRECT_BLOCKS_COUNT;

  if(index<limit){
    return inode_disk->direct_map_table[index];
  }
  
  // 2. indirect
  start = limit;
  limit += INDIRECT_BLOCKS_PER_SECTOR;

  struct inode_indirect_block_sector* indirect_disk;

  if(index < limit){
    indirect_disk = calloc(1, sizeof(struct inode_indirect_block_sector));
    buffer_cache_read(inode_disk->indirect_block, indirect_disk);

    temp = indirect_disk->blocks[index - start];
    free(indirect_disk);
    return temp;
  }

  // 3. double indirect

  start = limit;
  limit += INDIRECT_BLOCKS_PER_SECTOR * INDIRECT_BLOCKS_PER_SECTOR;

  struct inode_indirect_block_sector* double_disk;
  if(index < limit){
    off_t index1 = (index - start) / INDIRECT_BLOCKS_PER_SECTOR;
    off_t index2 = (index - start) % INDIRECT_BLOCKS_PER_SECTOR;
    
    double_disk = calloc(1, sizeof(struct inode_indirect_block_sector));
    indirect_disk = calloc(1, sizeof(struct inode_indirect_block_sector));
    // fetch two indirect block sectors
    buffer_cache_read (inode_disk->double_indirect_block, double_disk);
    buffer_cache_read (double_disk->blocks[index1], indirect_disk);
    temp = indirect_disk->blocks[index2];
    free(indirect_disk);
    free(double_disk);
    return temp;
  }

  NOT_REACHED();
  return -1;

}

bool inode_extend(struct inode_disk *disk_inode, off_t length, off_t old_length);
bool inode_extend_indirect(block_sector_t* p_entry, size_t num_sectors, size_t old_sectors);
bool free_inode(struct inode *);
bool inode_extend_double (block_sector_t* p_entry, size_t num_sectors, size_t old_sectors);


bool inode_extend(struct inode_disk *disk_inode, off_t length, off_t old_length){
  
  ASSERT(length>=0 && old_length >= 0);
  static char zero[BLOCK_SECTOR_SIZE];

  // get num of sectors need for length
    size_t num_sectors = bytes_to_sectors(length);
    size_t old_sectors = bytes_to_sectors(old_length);
    size_t i, k, t;
  
    // direct 부터 배정 (0~122)
    k = min(num_sectors, DIRECT_BLOCKS_COUNT);
    t = min(old_sectors, DIRECT_BLOCKS_COUNT);
    for(i=t;i<k;i++){
      if(disk_inode->direct_map_table[i] == 0){
        // 비어있는 경우
        if(!free_map_allocate(1, &disk_inode->direct_map_table[i]))
          return false;
        buffer_cache_write (disk_inode->direct_map_table[i], zero);
      }
    }
   
    num_sectors -= k;
    old_sectors -= t;
    if(!num_sectors) return true;

    // direct로 안 되면 indirect 배정 (123 ~ 250)
    k = min(num_sectors, INDIRECT_BLOCKS_PER_SECTOR);
    t = min(old_sectors, INDIRECT_BLOCKS_PER_SECTOR);
    if(! inode_extend_indirect (& disk_inode->indirect_block, k, t))
      return false;

    num_sectors -= k;
    old_sectors -= t;
    if(!num_sectors) return true;


    // indirect 로도 안 되면 double indirect block을 이용해서 배정
    // (대략 250 부터 시작)

    k = min(num_sectors, INDIRECT_BLOCKS_PER_SECTOR*INDIRECT_BLOCKS_PER_SECTOR);
    t = min(old_sectors, INDIRECT_BLOCKS_PER_SECTOR*INDIRECT_BLOCKS_PER_SECTOR);
    if(! inode_extend_double (& disk_inode->double_indirect_block, k, t)){
      return false;
    }
    num_sectors -= k;
    old_sectors -= t;
    if(!num_sectors) return true;

    ASSERT(num_sectors==0);
    ASSERT(old_sectors==0);
    return false;
}

// extend inode disk using double indirect block
bool inode_extend_double (block_sector_t* p_entry, size_t num_sectors, size_t old_sectors){
   
  static char zeros[BLOCK_SECTOR_SIZE];
  struct inode_indirect_block_sector double_block;

  if(*p_entry == 0) {
    // not yet allocated: allocate it, and fill with zero
    free_map_allocate (1, p_entry);
    buffer_cache_write (*p_entry, zeros);
  }
  // double_block에 p_entry read
  buffer_cache_read(*p_entry, &double_block);
  
  size_t i = 0, j, l, t;
  
  // do not check blocks which are allocated before
  t = old_sectors / INDIRECT_BLOCKS_PER_SECTOR;
  i = t; 
  num_sectors -= t*INDIRECT_BLOCKS_PER_SECTOR;
  old_sectors -= t*INDIRECT_BLOCKS_PER_SECTOR;
  t = old_sectors;
  // for indirect block used in double indirect
  struct inode_indirect_block_sector indirect;

  while(num_sectors>0){
    // l : num of sectors should be allocated or found already allocated in one loop.
    l = min(num_sectors, INDIRECT_BLOCKS_PER_SECTOR);
    // not yet allocatd, allocate and fiil with zero
    if(double_block.blocks[i] == 0){
      free_map_allocate (1, &double_block.blocks[i]);
      buffer_cache_write (double_block.blocks[i], zeros); 
    }
    // read double blocks's pointing block to indirect 
    buffer_cache_read(double_block.blocks[i], &indirect);

    for(j=t;j<l;j++){
      if(indirect.blocks[j] == 0){
        if(!free_map_allocate(1, &indirect.blocks[j])){
          return false;
        }
        buffer_cache_write(indirect.blocks[j], zeros);
      }
      num_sectors--;
    }

    // no indirected pointed data blocks now.
    t = 0; num_sectors -= t;
    // update double_block.blocks[i] with indirect
    buffer_cache_write(double_block.blocks[i], &indirect);

    i++;
  }
  ASSERT (num_sectors == 0);
  buffer_cache_write (*p_entry, &double_block);  
  return true;
}

bool inode_extend_indirect (block_sector_t* p_entry, size_t num_sectors, size_t old_sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];

  struct inode_indirect_block_sector indirect_block;
  if(*p_entry == 0) {
    // not yet allocated: allocate it, and fill with zero
    free_map_allocate (1, p_entry);
    buffer_cache_write (*p_entry, zeros);
  }
  buffer_cache_read(*p_entry, &indirect_block);

  size_t i, l = num_sectors, t = old_sectors;

  for(i=t;i<l;i++){
    if(indirect_block.blocks[i]==0){
        if(!free_map_allocate(1, &indirect_block.blocks[i]))
            return false;
        buffer_cache_write(indirect_block.blocks[i], zeros);
    }
    num_sectors--;
  }
  num_sectors -= t;
  old_sectors -= t;

  ASSERT (num_sectors == 0);
  buffer_cache_write (*p_entry, &indirect_block);
  return true;
}


bool free_inode(struct inode* inode){
  
  off_t file_length = inode->data.length;
  ASSERT(file_length >= 0);

  // alloc의 역순으로 제거
  size_t num_sectors = bytes_to_sectors(file_length);
  size_t i, l;

  // direct blocks
  l = min(num_sectors, DIRECT_BLOCKS_COUNT * 1);
  for (i = 0; i < l; ++ i) {
    free_map_release (inode->data.direct_map_table[i], 1);
    num_sectors--;
  }
  if(num_sectors == 0) return true;

  // indirect block
  l = min(num_sectors, INDIRECT_BLOCKS_PER_SECTOR);
  struct inode_indirect_block_sector indirect_block;
  buffer_cache_read(inode->data.indirect_block, &indirect_block);

  for(i=0;i<l;i++){
    free_map_release(indirect_block.blocks[i], 1);
    num_sectors--;
  }
  free_map_release(inode->data.indirect_block, 1);
  if(num_sectors == 0) return true;

  // double indirect block
  size_t j;
  struct inode_indirect_block_sector double_block;
  buffer_cache_read(inode->data.double_indirect_block, &double_block);
  i = 0;
  while(num_sectors > 0){
    l = min(num_sectors, INDIRECT_BLOCKS_PER_SECTOR);
    buffer_cache_read(double_block.blocks[i], &indirect_block);
    for(j=0;j<l;j++){
      free_map_release(indirect_block.blocks[j], 1);
      num_sectors--;
    }
    free_map_release(double_block.blocks[i], 1);
    i++;
  }
  free_map_release(inode->data.double_indirect_block , 1);
  if(num_sectors == 0) return true;

  ASSERT (num_sectors == 0);
  return false;
  
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (0 <= pos && pos < inode->data.length) {
    // sector index
    off_t index = pos / BLOCK_SECTOR_SIZE;
    return index_to_sector (&inode->data, index);
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isdir = isdir;
      // allocate inode for this disk_inode
      if(inode_extend(disk_inode,length, 0)){
        // if success update sector with disk_inode
        buffer_cache_write(sector, disk_inode);
        success = true;
      }      
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  //block_read (fs_device, inode->sector, &inode->data);
  buffer_cache_read(inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      //printf("inode should not be closed yet\n");
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        { 
          //printf("deallocated block and inode\n");
          free_map_release (inode->sector, 1);
          free_inode(inode);
        }
      //printf("freed inode\n");
      free (inode); 
    }
  //printf("inode->open_cnt is now %d\n");
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);

      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      
      if (chunk_size <= 0){
        break;
      }
      
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          buffer_cache_read(sector_idx, buffer+bytes_read);
          //block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
        
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL){
                break;
              }
            }
          //block_read (fs_device, sector_idx, bounce);
          buffer_cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  // inode_create

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  
  // beyond the EOF: extend the file
  if(offset+size > inode->data.length) {
    // extend and reserve up to [offset + size] bytes
    bool success;
    success = inode_extend (& inode->data, offset + size, inode->data.length);
    if (!success) return 0;  // fail?
    // write back the (extended) file size
    inode->data.length = offset + size;
    buffer_cache_write (inode->sector, & inode->data);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;
      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write (sector_idx, buffer + bytes_written);
          //block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) {
            buffer_cache_read(sector_idx, bounce);
            //block_read (fs_device, sector_idx, bounce);
          }
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          buffer_cache_write(sector_idx, bounce);
          //block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
