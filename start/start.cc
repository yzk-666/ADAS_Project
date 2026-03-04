#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

pid_t safebelt_fork = 0;
pid_t person_fork = 0;

// 信号处理函数
void signal_handler(int signum) {
    printf("收到信号 %d,准备终止子进程\n", signum);
    
    if (safebelt_fork > 0) {
        printf("终止安全带检测子进程 %d\n", safebelt_fork);
        kill(safebelt_fork, SIGTERM);
    }

    if (person_fork > 0) {
        printf("终止行人检测子进程 %d\n", person_fork);
        kill(person_fork, SIGTERM);
    } 
    exit(0);
}

int main(int argc, char const *argv[]) {
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill命令
    signal(SIGHUP, signal_handler);   // 终端断开
    
    // 创建安全带检测进程
    safebelt_fork = fork();
    if (safebelt_fork < 0) {
        perror("safebelt_fork error");
        exit(-1);
    } 
    else if (safebelt_fork == 0) {
        printf("这是子进程打印信息 <pid: %d, 父进程 pid: %d>\n", 
               getpid(), getppid());

        //切换到安全带检测文件夹
        if (chdir("../../sfbtdet/build") != 0) {
                perror("chdir failed");
                exit(1);
            }        
        execl("./adas", "adas", NULL);
        
        perror("execl执行失败");
    exit(1);  
    }
    
    person_fork = fork();
    if (person_fork < 0) {
        perror("person_fork error");
        exit(-1);
    } 
    else if (person_fork == 0) {
        printf("这是子进程打印信息 <pid: %d, 父进程 pid: %d>\n", 
               getpid(), getppid());

        //切换到行人检测文件夹
        if (chdir("../../Perdet/build") != 0) {
                perror("chdir failed");
                exit(1);
            }        
        execl("./perdet", "perdet", NULL);
        
        perror("execl执行失败");
    exit(1);  
    }
    printf("这是父进程打印信息 <pid: %d, 子进程 pid: %d>\n", 
            getpid(), safebelt_fork);
        
    int status;
    pid_t wait_pid = waitpid(safebelt_fork, &status, 0);
        
    if (wait_pid == -1) {
        perror("waitpid失败");
    } else {
        if (WIFEXITED(status)) {
            printf("子进程正常退出，退出码: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("子进程被信号终止，信号编号: %d\n", WTERMSIG(status));
        }
    }

    int status2;
    pid_t wait_pid2 = waitpid(person_fork, &status2, 0);
        
    if (wait_pid2 == -1) {
        perror("waitpid失败");
    } else {
        if (WIFEXITED(status2)) {
            printf("子进程正常退出，退出码: %d\n", WEXITSTATUS(status2));
        } else if (WIFSIGNALED(status2)) {
            printf("子进程被信号终止，信号编号: %d\n", WTERMSIG(status2));
        }
    }
        
    printf("父进程结束\n");
    return 0;
}