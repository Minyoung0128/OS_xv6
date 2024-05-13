#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"
#include "mmu.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

int main(int argc, char *argv[])
{
     // 테스트할 메모리 크기
     int size = PGSIZE * 100;

     // 가용 메모리 페이지 수 확인
     int free_pages_before = freemem();
     printf(1, "Free memory pages before mmap: %d\n", free_pages_before);

     // 메모리 매핑
     char *mapped = (char *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
     if (mapped == 0)
     {
          printf(2, "mmap failed\n");
          exit();
     }

     // 매핑된 메모리에 쓰기
     for (int i = 0; i < size; ++i)
     {
          mapped[i] = (char)(i & 0xFF);
     }

     // 데이터가 올바른지 확인
     for (int i = 0; i < size; ++i)
     {
          if (mapped[i] != (char)(i & 0xFF))
          {
               printf(2, "memory check failed at index %d\n", i);
               munmap((uint)mapped);
               exit();
          }
     }
     printf(1, "Memory check passed\n");

     // 가용 메모리 페이지 수 다시 확인
     int free_pages_after_alloc = freemem();
     printf(1, "Free memory pages after mmap: %d\n", free_pages_after_alloc);

     // 메모리 매핑 해제
     if (munmap((uint)mapped) < 0)
     {
          printf(2, "munmap failed\n");
          exit();
     }

     // 메모리 매핑 해제 후 가용 메모리 페이지 수 확인
     int free_pages_after = freemem();
     printf(1, "Free memory pages after munmap: %d\n", free_pages_after);

     // 테스트 결과 출력
     if (free_pages_before != free_pages_after)
     {
          printf(2, "Memory leak detected\n");
     }
     else
     {
          printf(1, "Memory allocation and deallocation successful\n");
     }

     exit();
}
