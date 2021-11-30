// This program sends data to a client
#include "common.h"

#include <stdbool.h>
#include <errno.h>
#include <iso646.h>
#include <limits.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

enum { WRITE_INDEX = 1 };

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [file_name]\n", argv[0]);
        return -1;
    }

    const char* file_name = argv[1];
    DEBUG_PRINT("Try to open %s\n", file_name);
    int in = open(file_name, O_RDONLY);
    if (in < 0) {
        perror("Failed to open input file");
        return -1;
    }
    DEBUG_PRINT("Opened %s\n", file_name);

    int tsem_mode = 0666;
    key_t tkey = ftok(semaphore_name, 1);
    DEBUG_PRINT("Shm sem set key: %d\n", tkey);
    DEBUG_PRINT("Create shm sem set\n");
    errno = 0;
    int tsem = semget(tkey, 2, IPC_CREAT | IPC_EXCL | tsem_mode);
    if (!errno) {
        DEBUG_PRINT("Populate shm sem set\n");
        union {
            int              val;
            struct semid_ds *buf;
            unsigned short  *array;
            struct seminfo  *__buf;
        } val;
        val.val = 1;
        semctl(tsem, 0, SETVAL, val);
        semctl(tsem, 1, SETVAL, val);
    }
    else if (errno == EEXIST) {
        DEBUG_PRINT("Open shm sem set\n");
        tsem = semget(tkey, 0, 0);
    }
    else {
        perror("Failed to create shm sem set");
        return -1;
    }

    {
        DEBUG_PRINT("Decrement shm sem\n");
        struct sembuf top = {
            .sem_num = WRITE_INDEX,
            .sem_op = -1,
            .sem_flg = 0,
        };
        semop(tsem, &top, 1);
    }

    key_t skey = ftok(semaphore_name, 2);
    DEBUG_PRINT("Sync sem set key: %d\n", skey);
    DEBUG_PRINT("Open sync sem set\n");
    int ssem = semget(skey, 1, IPC_CREAT | 0666);
    DEBUG_PRINT("Sync sem value: %d\n", semctl(ssem, 0, GETVAL));
    DEBUG_PRINT("Open shm\n");
    int sm = shmget(ftok(SHARED_MEMORY_NAME, 1), sizeof(ShmBuffer), IPC_CREAT | 0666);
    if (sm < 0) {
        perror("Failed to create shm");
        return -1;
    }
    DEBUG_PRINT("Attach shm\n");
    ShmBuffer* mem = shmat(sm, NULL, 0);

    while(true) {
        {
            DEBUG_PRINT("Wait for zero on sync sem\n");
            struct sembuf sop = {
                .sem_num = 0,
                .sem_op = 0,
                .sem_flg = 0,
            };
            semop(ssem, &sop, 1);
            DEBUG_PRINT("Sync sem value: %d\n", semctl(ssem, 0, GETVAL));
        }

        DEBUG_PRINT("Write chunk\n");
        mem->size = read(in, mem->buffer, sizeof(mem->buffer));

        {
            DEBUG_PRINT("Increment sync sem\n");
            struct sembuf sop = {
                .sem_num = 0,
                .sem_op = 1,
                .sem_flg = 0,
            };
            semop(ssem, &sop, 1);
            DEBUG_PRINT("Sync sem value: %d\n", semctl(ssem, 0, GETVAL));
        }

        if (!mem->size) {
            DEBUG_PRINT("EOF\n");
            break;
        }
    }

    {
        DEBUG_PRINT("Increment shm sem\n");
        struct sembuf top = {
            .sem_num = WRITE_INDEX,
            .sem_op = 1,
            .sem_flg = 0,
        };
        semop(tsem, &top, 1);
    }
}
