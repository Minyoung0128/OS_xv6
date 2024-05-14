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
		int f;

		if((f=fork())==0){
            printf(1,"------ children -------\n");
            for(int j = 0; j<50;j++){
                printf(1,"%dth memory mapping\n", j);
                int result = mmap(PGSIZE *(j*2),PGSIZE * 2,PROT_READ|PROT_WRITE,MAP_POPULATE,fd,0); // 파일 있고 not populated
                if(j%1 == 0){
                    munmap(result);
                }
                printf(1,"freemem now is %d\n\n",freemem());
            }
		}
		
		else{
			wait();
            printf(1,"------ Parent -------\n");
			for(int j = 0; j<50;j++){
                printf(1, "%dth memory mapping\n", j);
                int result = mmap(PGSIZE*(j*2), PGSIZE * 2,PROT_READ|PROT_WRITE, MAP_POPULATE, fd, 0); // 파일 있고 not populated
                if(j%1 == 0){
                    munmap(result);
                }
                if(result == 0){
                    printf(1, "There is No PTE\n");
                    break;
                }
                printf(1,"freemem now is %d\n\n",freemem());
            }
		}
		// printf(1,"Lastly freemem now is %d\n",freemem());
        close(fd);
		exit();
        return 0;
}