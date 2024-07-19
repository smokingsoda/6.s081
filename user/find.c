#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void file_tree_recursion(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        if (strcmp(path + strlen(path) - strlen(target), target) == 0) {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (strcmp(buf + strlen(buf) - 2, "/.") != 0 &&
                strcmp(buf + strlen(buf) - 3, "/..") != 0) {
                file_tree_recursion(buf, target); // 递归查找
            }
        }
        break;
    }
    close(fd);
    return;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "usage: find [path] target\n");
        exit(1);
    }
    char target[512];
    target[0] = '/';
    strcpy(target + 1, argv[2]);
    file_tree_recursion(argv[1], target);
    exit(0);
}
