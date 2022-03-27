#include <stddef.h>

#define SECTORS_PER_PAGE 8
#define SWAP_FREE 0
#define SWAP_USED 1

// swap 영역 초기화
void swap_init(void);

// swap_in : used_index의 swap slot에 저장된 데이터를 kaddr로 복사
void swap_in(size_t used_index, void* kaddr);

// kaddr 주소가 가리키는 페이지를 swap partition에 기록
// page를 기록한 swap slot 번호를 return
size_t swap_out(void* kaddr);

