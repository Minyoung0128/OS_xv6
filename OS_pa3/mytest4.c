#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"
#include "mmu.h"


int main(int argc, char *argv[])
{
    // page fault handling 테스트
    int fd = open("README",O_RDWR);

    printf(1,"fd is %d\n",fd);
    printf(1,"FIRST freemem now is %d\n",freemem());

    char* idx1 = (char*) mmap(PGSIZE * 4, PGSIZE * 2, PROT_READ|PROT_WRITE , MAP_ANONYMOUS, -1, 0); // 파일 없이 not populate
    printf(1,"mmap without populate freemem now is %d\n",freemem());

    printf(1,"Test Page Fault handler without file\n");
    *(idx1+PGSIZE) = 1;
    printf(1, "After Page Fault Handler : %d\n\n", freemem());
    
    printf(1,"mmap without populate freemem now is %d\n",freemem());
    char* idx2 =(char*) mmap(PGSIZE * 6,PGSIZE * 2,PROT_READ|PROT_WRITE,0,fd,0); // 파일 있고 not populated
    printf(1,"Test Page Fault handler with file\n");
    int i = *(idx2+PGSIZE);
    printf(1,"i : %d\n",i);
    
    printf(1, "After Page Fault Handler : %d\n\n", freemem());

    munmap(idx1);
    munmap(idx2);

    exit();
}