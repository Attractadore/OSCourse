// This program reads data sent by the server and prints it
#include "common.h"

#include <unistd.h>

int main() {
    SharedMemory* shm = getSharedMemory();
    if (!shm) {
        fprintf(stderr, "Failed to open shm\n");
        return -1;
    }

    int sem_set = getSemaphoreSet();
    if (sem_set == -1) {
        fprintf(stderr, "Failed to open sem set\n");
        return -1;
    }

    int res = acquireClient(sem_set);
    if (res == -1) {
        fprintf(stderr, "Failed to acquire from client\n");
        return -1;
    }

    res = copyFromSharedMemory(sem_set, shm, STDOUT_FILENO);
    if (res == -1) {
        fprintf(stderr, "Failed to recieve from server\n");
        return -1;
    }
}
