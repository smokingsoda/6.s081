#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
    if (argc > MAXARG) {
        fprintf(2, "Too many arguments!\n");
        exit(1);
    }

    char *args[MAXARG];
    for (int i = 0; i < argc; i++) {
        args[i] = argv[i];
    }

    char **argp = args + argc;
    int argNum = argc;

    char buf[1024];
    char *p, *head;
    p = buf;
    head = p;

    while (read(0, p, sizeof(char)) != 0) {
        if (*p == '\n') {
            if (argNum > MAXARG) {
                fprintf(2, "Too many arguments\n");
                exit(1);
            }
            /* clear the buffer */
            *p = 0;
            *argp = head;
            head = p + 1;
            argNum += 1;
            argp += 1;
        }
        p++;
    }

    if (fork() == 0) {
        exec(args[1], args + 1);
    } else {
        wait(0);
        exit(0);
    }
    return 0;
}