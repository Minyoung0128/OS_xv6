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
        int fd = open("README",O_RDWR);
        printf(1,"fd is %d\n",fd);
        printf(1,"FIRST freemem now is %d\n",freemem());

		uint test1 = mmap(0, PGSIZE * 2, PROT_READ|PROT_WRITE,MAP_POPULATE, fd, 0); // 파일 읽어오고 populated

        printf(1,"SECOND freemem now is %d\n",freemem());
        uint test2 = mmap(PGSIZE * 2, PGSIZE * 2, PROT_READ , MAP_ANONYMOUS|MAP_POPULATE, -1, 0); // 파일 없이 populated
 
        printf(1,"THIRD freemem now is %d\n",freemem());
        uint test3 = mmap(PGSIZE * 4, PGSIZE * 2, PROT_READ ,MAP_ANONYMOUS, -1, 0); // 파일 없이 not populate

        printf(1,"FOURTH freemem now is %d\n",freemem());
        uint test4 = mmap(PGSIZE * 6,PGSIZE * 2,PROT_READ|PROT_WRITE,0,fd,0); // 파일 있고 not populated
        
        //printf(1,"@@@@@@@@@@@@@@@@@@@@@@forth@@@@@@@\n%s\n",test4);
        printf(1,"!!finish\n");

        printf(1,"LAST freemem now is %d\n",freemem());
		// printf(1, "%x %x %x %x\nGO to munmap!\n", (uint)test, (uint)test2, (uint)test3, (uint)test4);

		int i = read(fd, (void*)test1, 0);
		printf(1,"-- read result : %d\n",i);
		int f;
		if((f=fork())==0){
			printf(1, "----------- CHILD START --------------\n");
			int x;
			printf(1,"first unmap : %d\n",test1);
			x = munmap(test1);
			printf(1,"0: %d unmap results\n", x);
			printf(1,"freemem now is %d\n",freemem());
			
			printf(1,"second : %d \n", test2);
			x = munmap(test2);
			printf(1,"4096: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
			printf(1,"third : %d\n",test3);
			x = munmap(test3);
			printf(1,"8192: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
			
			// printf(1,"fourth%d \n",0x40000000+(PGSIZE * 200));
			x = munmap(test4);
			x = munmap(0x40000000+(PGSIZE * 200));
			printf(1,"16384: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
		
		}
		
		else{
			wait();
			printf(1, "-------- PARENT START ----------\n");
			int x;
			printf(1,"first\n");
			x = munmap(test1);
			printf(1,"0: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
			printf(1,"second\n");
			x = munmap(test2);
			printf(1,"4096: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
			printf(1,"third\n",test3);
			x = munmap(test3);
			printf(1,"8192: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());

			printf(1,"fourth\n",test4);
			x = munmap(test4);
			printf(1,"16384: %d unmap results\n",x);
			printf(1,"freemem now is %d\n",freemem());
			
		}
		
		printf(1,"Lastly freemem now is %d\n",freemem());
        close(fd);
		exit();
        return 0;
}