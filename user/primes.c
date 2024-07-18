#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void recursion(int left_pip_fd[2]) {
    int p, status;
    read(left_pip_fd[0], &p, sizeof(int));
    if (p == -1) {
        exit(0);
    } else {
        printf("prime %d\n", p);
        int current_pip_fd[2];
        pipe(current_pip_fd);
        if (fork() == 0) {
            // child process
            close(current_pip_fd[1]);
            // debug function

            //printf("process id is %d, left_pip_fd[0] is %d, left_pip_fd[1] is %d\n", getpid(), left_pip_fd[0], left_pip_fd[1]);
            //printf("close left_pid_fd[0] result is %d\n", close(left_pip_fd[0]));
            //printf("close left_pid_fd[1] result is %d\n", close(left_pip_fd[1]));
            
            recursion(current_pip_fd);
        } else {
            int n = 0;
            close(current_pip_fd[0]);
            while (read(left_pip_fd[0], &n, sizeof(int)) && n != -1) {
                if (n % p != 0) {
                    write(current_pip_fd[1], &n, sizeof(int));
                }
            }
            n = -1;
            write(current_pip_fd[1], &n, sizeof(int));
            wait(&status);
        }
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage: primes\n");
        exit(1);
    } else {
        int init_pip_fd[2];
        int status;
        pipe(init_pip_fd);

        if (fork() == 0) {
            close(init_pip_fd[1]);
            recursion(init_pip_fd);
            exit(0);
        } else {
            close(init_pip_fd[0]);
            for (int i = 2; i <= 35; i++) {
                write(init_pip_fd[1], &i, sizeof(int));
            }
            int i = -1;
            write(init_pip_fd[1], &i, sizeof(int));
            wait(&status);
            exit(0);
        }
    }
}