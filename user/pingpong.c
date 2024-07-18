#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc != 1) {
        fprintf(2, "usage: pingpong\n");
        exit(1);
    } else {
        int pip_fd[2];
        int status;
        pipe(pip_fd);
        int fork_pid = fork();
        if (fork_pid == 0) {
            // Child process
            char buf;
            read(pip_fd[0], &buf, 1);
            printf("%d: received ping\n", getpid());
            write(pip_fd[1], "B", 1);
        } else {
            // Parent process
            write(pip_fd[1], "A", 1);
            wait(&status);
            char buf;
            read(pip_fd[1], &buf, 1);
            printf("%d: received pong\n", getpid());
        }
        exit(0);
    }
}