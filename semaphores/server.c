// This program sends data to a client
#include "common.h"

#include <fcntl.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [file_name]\n", argv[0]);
        return -1;
    }

    const char* file_name = argv[1];
    DEBUG_PRINT("Open %s\n", file_name);
    int in = open(file_name, O_RDONLY);
    if (in < 0) {
        perror("Failed to open input file");
        return -1;
    }
    DEBUG_PRINT("Opened %s\n", file_name);
    DEBUG_PRINT("\n");

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

    int res = acquireServer(sem_set);
    if (res == -1) {
        fprintf(stderr, "Failed to acquire from server\n");
        return -1;
    }

    res = copyIntoSharedMemory(sem_set, shm, in);
    if (res == -1) {
        fprintf(stderr, "Failed to send to client\n");
        return -1;
    }
}
