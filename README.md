# Pintos

서강대학교 OS Project Pintos 

위 레포지토리의 내용들은 서강대학교 OS 수업에서 제출된 프로젝트들로, 그대로 복사하여 제출시 카피 처리를 받을 확률이 굉장히 높습니다. 참고용으로만 봐주시길 바랍니다.

Proj 1: User Program (1)
-	핀토스 OS에서 Argument passing, User memroy Access 등을 구현하고, 이를 기반으로 이미 구현되어 있던 System call handler API를 이용하여, Kernel에서의 System call handler를 구현한다. 

Proj 2: User Program (2)
- Proj1에서 미처 다 구현하지 못한 Sytem Call 들을 모두 구현하는 것이 목적이다.
- File Descriptor, create(), remove(), open(), close() 등의 파일 관련 System Call 

Proj 3: Threads 
-  Alarm Clock을 구현하여 Thread의 Scheduling을 개선하여 CPU의 cycle과 전력 소모를 절약한다.
-  Proj2 까지의 핀토스는 Thread Scheduling을 단순 Round Robin 방식으로 처리하고 있는데, 이를 Priority 기반 scheduling으로 개선하고 추가구현으로 다시 BSD Scheduler를 사용한 scheduling으로 대체한다.

Proj 4: Virtual Memory
- 추가적인 Page Table과 이를 이용한 Page Fault 발생시의 Page Fault Handler을 구현하고, LRU 정책을 사용하는 Page-disk Swap을 구현한다. Page size를 벗어나는 Stack에 대한 접근이 와도 대처할 수 있도록 Stack Growth를 구현한다.

Proj 5: File System
- 기존의 핀토스 파일 시스템을 multi-indexed allocation 방식으로 변경하여 구현한다. 또, 이를 기반으로 sub-directory를 지원한다.
- 기존의 파일 시스템은 buffer cache 없이 모든 변화를 바로바로 disk에 적용했는데 이를 buffer cache를 구현하여 개선한다. 
- mkdir, readdir 등의 새로운 syscall을 구현한다.

