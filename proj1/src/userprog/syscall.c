#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <stdlib.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "threads/vaddr.h"
#include "threads/thread.h"


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_user_address(void* addr){

  if(!is_user_vaddr(addr)){
    //printf("user addr fault : addr(%d)\n", (int)addr);
    exit(-1);
  }
}

void check_kernel_address(void* addr){
   if(!is_kernel_vaddr(addr)){
    //printf("kernel addr fault : addr(%d)\n", (int)addr);
    exit(-1);
  }

}

// 현재 pintos는 system call handler가 구현되어 있지 않아 시스템 콜이 호출될 수 없어
// user program이 정상적으로 동작하지 않는 상태이다.

// System call이란 user program이 작동하는데 있어 커널 기능을 사용할 수 있도록 운영 체제가 제공하는 인터페이스이다.
// 예를 들어, 특정 user program이 작동하면서 메모리 읽기 및 쓰기와 같은 커널 기능에 대한 request를 보내면, 
// 커널 영역에서 시스템 콜이 실행되어 처리 후 결과 값을 넘겨주는 식입니다. 
// 즉, 프로젝트1에서 외부 timer 및 I/O device로부터의 interrupt를 처리하는 것과 같이, 
// 소프트웨어 내부에서 발생하는 interrupt 및 exception를 처리하기 위한 기능인 것입니다.

// fibonacci, max_of_four_int 추가 구현 필요
// read, write의 경우 special case..?
// read, write가 STDIN/ STDOUT 만을 가정하고 만드는 것을 의미하는듯?

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  //printf("system num : %d\n", *(uint32_t *)(f->esp));
  int i;

  check_user_address(f->esp);

  //hex_dump((uint32_t)(f->esp), (f->esp), 100, 1);

  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      // parent 프로세스가 wait 하는 경우 
      // process_exit에서 status 출력이 있는데 그 부분이 도움이 될수도..?
      // child 프로세스가 exit할때 status return이 필요..
      // 차차 구현
      check_user_address(f->esp+4);
      exit((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_EXEC:
      // 다행히 PPT에 process_execute() in userprog/process.c 를 참조하라고 나와있다.
      // 코드를 살펴보니 thread를 생성하고, 그 thread에서는 start_process를 호출해서 load 등등을 해준다..
      // 그냥 cmd 라인을 그대로 넘겨주면 될듯?
      // 근데 인자로 받은건 f 뿐인데..?

      // 우리가 건드린 것은..? f의 esp, eip 정도
      // stack에는 각 argument와 그 주소 등등이 들어있음

      // manual을 한번 정독해보자..
      //printf("in sys exec\n");
      //printf("%s\n", (char *)(uint32_t *)(f->esp+4));
      //hex_dump(f->esp, f->esp, 100, 1);
      check_user_address(f->esp+4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_WAIT:
      //hex_dump(f->esp, f->esp, 100, 1);
      check_user_address(f->esp+4);
      f->eax = wait((pid_t)*(uint32_t *)(f->esp+4));
      break;
    case SYS_CREATE:
      // 아직 구현 요구 사항에 없음
      break;
    case SYS_REMOVE:
      // 미구현
      break;
    case SYS_OPEN:
      // 미구현
      break;
    case SYS_FILESIZE:
      // 미구현
      break;
    case SYS_READ:
      for(i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      //hex_dump(f->esp, f->esp, 100, 1);
      //printf("from Read: %s\n", (char*)(*(uint32_t *)(f->esp+8)));
      f->eax = read((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_WRITE:
    // 주소에 들어있는 값을 가져오면 된다!! 내일 해결
      for(i=1;i<=3;i++)
        check_user_address(f->esp+i*4);
      //hex_dump(f->esp, f->esp, 100, 1);
      //printf("from Write: %s\n", (char*)(*(uint32_t *)(f->esp+8)));
      f->eax = write((int)(*(uint32_t *)(f->esp+4)),  (void*)(*(uint32_t *)(f->esp+8)), (unsigned)(*(uint32_t *)(f->esp+12)));
      break;
    case SYS_FIBONACCI:
      check_user_address(f->esp+4);
      f->eax = fibonacci((int)(*(uint32_t *)(f->esp+4)));
      break;
    case SYS_MAX_OF_FOUR:
      for(i=1;i<=4;i++)
        check_user_address(f->esp+i*4);
      f->eax = max_of_four_int((int)(*(uint32_t *)(f->esp+4)),  (int)(*(uint32_t *)(f->esp+8)), (int)(*(uint32_t *)(f->esp+12)), (int)(*(uint32_t *)(f->esp+16)));
      break;
    case SYS_SEEK:
      // 미구현
      break;
    case SYS_TELL:
      // 미구현
      break;
    case SYS_CLOSE:
      // 미구현
      break;
  }

  // thread_exit ();

}

// What wait() system call should do is "wait child process until it finishes its work."
// - Check that child thread ID is valid
// - Get the exit status from child thread when the child thread is dead
// - To prevent termination of process before return from wait(), you can use busy waiting technique* or thread_yield() in threads/thread.c.


int wait(pid_t pid){
  // Waits for a child process pid and retrieves the child's exit status.
  return process_wait(pid);
}


void exit(int status){

  //struct thread *cur = thread_current();
  //cur->status = status;
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_current() -> exit_status = status;
  thread_exit();
}


int write(int fd, const void *buffer, unsigned size){
  // writes 'size' bytes from 'buffer' to the open file 'fd'
  // use 'putbuf()'
  if(fd==1){
    putbuf(buffer, size);
    return size;
  }
  else{
    printf("fd error from write!, fd should be 1, but fd(%d)\n", fd);
  }
  return -1;
}


int read (int fd, void* buffer, unsigned size){

  // returns actually read bytes or -1 when can't be read.
  // fd 0 reads from the keyboard using input_getc()
  // esp+4 | esp+8 | esp+12 이렇게 읽힌다

  int i;
  if (fd==0){
    for(i=0;i<size;i++){
      if((((char*)buffer)[i] = input_getc()) =='\0'){
        break;
      }
    }
  }
  else{
    printf("fd error from read!, fd should be 0, but fd(%d)\n", fd);
    return -1;
  }

  return size;

}

void halt(){
  // terminates pintos
  shutdown_power_off();
}

pid_t exec(const char *cmd_line){
  // runs the executable whos name is given in cmd_line,
  // passing any given arguments,
  // returns the new process's programs id(pid)
  // if program can't be load or run for any reason, must return -1
  return process_execute(cmd_line);
}

int fibonacci(int n){

  int *array, result, i;

  if(n<2)
    return 1;
  
  array = (int*)malloc(sizeof(int)*(n+1));
  array[0] = array[1] = 1;
  for(i=2;i<n;i++){
    array[i] = array[i-1] + array[i-2];
    //printf("array[%d]: %d\n", i, array[i]);
  }
  
  result = *(array+n-1);
  free(array);

  return result;
}

int max_of_four_int(int a, int b, int c, int d){

  int n1, n2, result;

  n1 = a>b ? a : b;
  n2 = c>d ? c : d;
  result = n1>n2 ? n1 : n2;

  return result;
}