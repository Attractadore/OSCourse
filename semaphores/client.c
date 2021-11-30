// This program reads data sent by the server and prints it
#include "common.h"

#include <unistd.h>

int main() {
    SharedMemory* shm = getSharedMemory();
    if (!shm) {
        fprintf(stderr, "Failed to open shm\n");
        return -1;
    }

    int i = selectClientBlock();
    if (i < 0) {
        fprintf(stderr, "Failed to select client block\n");
        return -1;
    }

    int sem_set = getBlockSemaphore(i);
    if (sem_set == -1) {
        fprintf(stderr, "Failed to open sem set\n");
        return -1;
    }

    copyFromSharedMemory(sem_set, getBlockBuffer(shm, i), STDOUT_FILENO);
}
