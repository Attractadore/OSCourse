#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>

void run(unsigned n) {
    key_t key = ftok(__FILE__, 1);
    int q = msgget(key, IPC_CREAT | 0600);
    long last = 0;
    for (unsigned i = 0; i < n; i++) {
        pid_t pid = fork();
        if (!pid) {
            if (i) {
                long msgtyp = i;
                msgrcv(q, &msgtyp, 0, msgtyp, 0);
            }
            fprintf(stdout, "%u\n", i);
            fflush(stdout);
            long msgtyp = i + 1;
            msgsnd(q, &msgtyp, 0, 0);
            exit(0);
        } 
        if (pid < 0) {
            break;
        }
        last = i + 1;
    }
    if (last) {
        msgrcv(q, &last, 0, last, 0);
        if (last < n) {
            perror("Failed to fork");
        }
    }
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s n\n", argv[0]);
        return -1;
    }
    char* end = NULL;
    long long n = strtoll(argv[1], &end, 10);
    if (*end or n <= 0) {
        fprintf(stderr, "N must be an non-negative integer\n");
        return -1;
    }
    if (errno == ERANGE or n > UINT_MAX) {
        fprintf(stderr, "N is too large. Use a smaller value\n");
        return -1;
    }
    run(n);
    return 0;
}
