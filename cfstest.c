#include "types.h"
#include "user.h"
#include "stat.h"

#define NCHILDREN 20

int cnt = 0;
unsigned long randstate = 1;
unsigned int rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

int main(int argc, char *argv[]) {
  int pid;
  int children[NCHILDREN] = {0};
  for(int i = 0; i < NCHILDREN; ++i) {
    if ((pid = fork()) > 0) {
      children[i] = pid;
    }
    if (pid < 0) {
      for(int j = 0; j < i; ++j) {
        kill(children[j]);
      }
      exit();
    }
    if (pid == 0) {
      while(1) {
        for(int i = 0; i < 10000000; ++i) {cnt = i;}
        //if (i%2==0) {;}
        //else {sleep(1);}
      }
    }
  }
  setnice(getpid(), 0);

  printf(1, "successfully forked children!\n");

  //sleep(100);
  for(int i = 0; i < NCHILDREN; ++i) {
    //int rnd = rand() % 40;
    //printf(1, "random number = %d\n", rnd);
    if (setnice(children[i], i * 2 + 1) < 0)
      printf(1, "failed to set nice of %d'th child\n", i);
    else
      printf(1, "nice value of %d'th child : %d\n", i, getnice(children[i]));
  }

  while(1) {
    ps(0);
    sleep(100);
    //for(int i = 0; i < 100000000; ++i) {cnt=0;}
  }
  exit();
}