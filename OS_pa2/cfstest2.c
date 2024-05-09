#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char ** argv) {
    double x = 0;
    int pid;
    int children[10] = {0};
    int wake_pid = -1;

    // 10개의 자식 프로세스 생성
    for(int i = 0; i < 10; ++i) {
        if ((pid = fork()) > 0) {
            children[i] = pid;
            setnice(pid, i * 2 + 1);
        }
        if (pid < 0) {
            // fork 실패 시, 생성된 자식 프로세스들을 종료시킴
            for(int j = 0; j < i; ++j) {
                kill(children[j]);
            }
            exit();
        }
        if (pid == 0) {
            // 특정 자식 프로세스는 일정 작업 수행 후 sleep에 진입
            if (i % 4 == 0) {
                printf(1, "Sleep process pid %d\n", getpid());
                for(int i = 0; i < 10000000; ++i) {
                    sleep(10);
                    if(i%100==0){
                        printf(1,"pid : %d\n",getpid());
                        ps(0);
                    }
                    x += 0.1;
                }
            } else {
                // 나머지 자식 프로세스들은 계속해서 연산을 수행
                while (1) {
                    // 연산을 수행하는 데 시간 소모
                    for (int cnt = 0; cnt < 10000000; ++cnt) {}
                }
            }
            exit(); // 프로세스 종료
        }
    }

    // 부모 프로세스는 특정 자식 프로세스가 sleep 상태에 들어간 후에도 계속해서 ps를 호출함
    if (getpid() != wake_pid) {
        while(1) {
            sleep(500);
            printf(1,"Wake up and Check ps\n");
            ps(0);
        }
    }

    // 깨어난 후, 다른 자식 프로세스들의 종료를 기다림
    for (int i = 0; i < 10; ++i) {
        wait();
    }

    exit();
}

