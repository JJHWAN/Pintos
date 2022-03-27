#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include <stdlib.h>

/* Partition that contains the file system. */
struct block *fs_device;

// print out the entry name of dir
void check_dir_entry(struct dir* dir){
  
  ASSERT(dir!=NULL);
  char entry[20];
  struct inode* inode = dir_get_inode(dir);
  struct dir* dir_child;

  if(!inode_is_directory(inode)){
      return false;
  }
  int i =0;
  while(dir_readdir(dir, entry) == true)
		{
      printf("entry %d : %s\n",i++, entry);
		}

}

static void do_format (void);

// Find the current directory with path_name
// whether it is absolute path, or relative path
// update file_name to file name in current dir
struct dir* parse_path (char* path_name, char* file_name){

  struct dir *dir = dir_open_root();
  struct inode *inode;

  if(path_name == NULL || file_name ==NULL)
    PANIC("path name or file name NULL");

  int l = strlen(path_name);
  if(l==0){
    return NULL;
  }

  // path name의 절대, 상대 경로에 따른 디렉토리 정보 저장
  char* dir_path = (char *) malloc(sizeof(char)*(l+1));

  if(path_name[0] == '/'){
    dir = dir_open_root();
  }
  else{
    ASSERT(thread_current()->t_dir !=NULL);
    if(inode_removed_or_not(dir_get_inode(thread_current()->t_dir))){
      return NULL;
    }
    dir = dir_reopen(thread_current()->t_dir);
  }
  
  char *token, *nextToken = "", *savePtr;
  token = strtok_r(path_name, "/", &savePtr);
  nextToken = strtok_r(NULL, "/", &savePtr);
  // token or nextToken NULL? no need to search more
  while(token != NULL && nextToken != NULL){
    // search dir with token, if not return null
    if(!dir_lookup(dir, token, &inode)){
      return NULL;
    }
    // if inode is not a dir -> something wrong 
    // inode should be dir or found in above dir_lookup
    if(!inode_is_directory(inode)){
      dir_close(dir);
      return NULL;
    }
    dir_close(dir);
    // upadte dir with inode -> to next dir 
    dir = dir_open(inode);
    // update token
    token = nextToken;
    nextToken = strtok_r(NULL, "/", &savePtr); 
  }
  // if token NULL -> for error detecting file_name == '\0';
  if(token == NULL){
    file_name[0] = '\0';
  } // update file_name
  else strlcpy(file_name, token, strlen(token)+1);

  return dir;
}

// name 경로에 dir 생성
bool filesys_create_dir (const char* name){

  struct dir *parent_dir;
	struct inode *parent_inode;
	struct inode *tmp;
	struct dir *new_dir;
	bool result = false;

	/* if dir name is NULL, return false*/
	if(name == NULL)
		return result;
  if(strlen(name)==0)
    return result;
	
  // String arrays for strtok_r (parse path)
	char *cp_name = malloc( sizeof(char) * (strlen(name)+1) );
	strlcpy(cp_name, name, strlen(name)+1 );
	char *file_name;
	file_name = malloc( sizeof(char) * (strlen(name)+1) );
	if(file_name == NULL)
	{
		free(cp_name);
		return result;
	}

	parent_dir = parse_path(cp_name, file_name);
  ASSERT(file_name!=NULL);
  ASSERT(parent_dir!=NULL);

	// same file name file exits, return false
	if(dir_lookup(parent_dir, file_name, &tmp) == true)
		return result;

	// allocate sector_dix from bitmap
	block_sector_t sector_idx;
	free_map_allocate(1, &sector_idx);

	// create dir
	if(!dir_create(sector_idx, 16)){
    result = false;
    goto done;
  }

	// add new entry to parent dir
  if(!check_memory()) goto done;
  new_dir = dir_open( inode_open(sector_idx) ); 
  if(new_dir==NULL){
    goto done;
  }

	dir_add(parent_dir, file_name, sector_idx);

  // add '.' and '..' for default
	dir_add(new_dir,".",sector_idx);
	parent_inode = dir_get_inode(parent_dir);
	dir_add(new_dir,"..", inode_get_inumber(parent_inode));
  dir_readdir(new_dir, file_name);

  result = true;

  done:
	  free(cp_name);
  	free(file_name);
	
	  return result;
}


/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  buffer_cache_init();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current()->t_dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_terminate();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  char *cp_name   = malloc( sizeof(char) * (strlen(name) + 1) );
  char *file_name = malloc( sizeof(char) * (strlen(name) + 1) ); 
  if( cp_name == NULL || file_name == NULL)
	  return false;

  block_sector_t inode_sector = 0;
  strlcpy(cp_name, name, strlen(name)+1);
  struct dir *dir = parse_path(cp_name, file_name);

  // get inode_sector and allocate bitmap
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector));

  // create inode with inode_sector
  success = success && inode_create (inode_sector, initial_size, 0);
  // add file to current directory
  success = success && dir_add (dir, file_name, inode_sector);
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  dir_close (dir);
  free(cp_name);
  free(file_name);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char *cp_name   = malloc( sizeof(char) * (strlen(name) + 1) );
  char *file_name = malloc( sizeof(char) * (strlen(name) + 1) ); 
  bool not_removed = 1;
  if( cp_name == NULL || file_name == NULL)
	  return false;
  strlcpy(cp_name, name, strlen(name)+1);

  // get current dir
  struct dir *dir = parse_path(cp_name, file_name);
  struct inode *inode = NULL;

  if (dir != NULL){
    if (strlen(file_name)>0) {
      // if file not removed, dir_lookup will return true
      not_removed = dir_lookup (dir, file_name, &inode);
      dir_close (dir);
    }
    else { // empty filename : just return the directory
      inode = dir_get_inode (dir);
    }
  }

  free(cp_name);
  free(file_name);

  if(!not_removed){
    return NULL;
  }
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{

  char *cp_name   = malloc( sizeof(char) * (strlen(name) + 1) );
  char *file_name = malloc( sizeof(char) * (strlen(name) + 1) ); 
  if( cp_name == NULL || file_name == NULL)
	  return false;

  strlcpy(cp_name, name, strlen(name)+1);
  struct dir *dir = parse_path(cp_name, file_name);

  bool success = (dir != NULL);
  success = success && dir_remove (dir, file_name);
  dir_close (dir); 

  free(file_name);
  free(cp_name);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  
  // add '.' and '..' to root directory
  struct dir* root = dir_open_root();
  struct inode* root_inode = dir_get_inode(root);
  dir_add(root, ".", inode_get_inumber(root_inode));
  dir_add(root,"..", inode_get_inumber(root_inode));
  free_map_close ();
  printf ("done.\n");
}
