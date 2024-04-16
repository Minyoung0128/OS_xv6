
#include "types.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char *argv[])
{
    int parent = getpid();
    int children[10]={0};
    int pid;

    for(int i = 0; i < 10; ++i) {
    if ((pid = fork()) > 0) {
        children[i] = pid;
        for(int j=0;j<100000;j++){
        printf(1,"Let's sleep%d\n",pid);
        ps(0);
        sleep(1000); 
        printf(1,"Wake up!%d\n",pid);
        ps(0);}
    }
  }
    if (getpid() == parent){
        while(1) {
    ps(0);
    sleep(100);
    //for(int i = 0; i < 100000000; ++i) {cnt=0;}
  }
    }

    for(int i = 0; i < 10; ++i) {
    if (setnice(children[i], i * 2 + 1) < 0)
      printf(1, "failed to set nice of %d'th child\n", i);
    else
      printf(1, "nice value of %d'th child : %d\n", i, getnice(children[i]));
  }

    exit();
}