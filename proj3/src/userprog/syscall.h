#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"
#include "threads/synch.h"

void syscall_init (void);

// 포인터 영역 검사
void check_user_address(void* addr);
void check_kernel_address(void* addr);
// halt 함수
void halt(void);

// write 함수
// fd 는 file descriptor : 아직은 STDOUT만 신경쓰면 된다.
// buffer는 레지스터에서 받아 나올 buffer?
int write(int fd, const void *buffer, unsigned size);

// read 함수
// fd 는 file descriptor : 아직은 STDIN만 신경쓰면 된다.
int read (int fd, void* buffer, unsigned size);

// exit 함수
// status는 아직 return할 일이 없는것 같음..
void exit(int status);

// exec
pid_t exec(const char *cmd_line);

// wait
int wait(pid_t pid);

int fibonacci(int n);

int max_of_four_int(int a, int b, int c, int d);

// ------------------- Proj2 -------------------------

bool create (const char *file, unsigned initial_size);

bool remove (const char *file);

int open (const char *file);

void close (int fd);

int filesize (int fd);

void seek (int fd, unsigned position); 

unsigned tell (int fd);


#endif /* userprog/syscall.h */
