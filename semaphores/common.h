#pragma once
#include <stdio.h>

#ifdef DEBUG 
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while(0)
#endif

typedef struct {
    ssize_t size;
    enum { BUFFER_SIZE = 4096};
    char buffer[BUFFER_SIZE];
} TransferBuffer;

typedef struct {
    TransferBuffer buffer;
} SharedMemory;

SharedMemory* getSharedMemory();
int getSemaphoreSet();

int acquireServer(int sem_set);
int acquireClient(int sem_set);

int copyIntoSharedMemory(int sem_set, SharedMemory* shm, int fd);
int copyFromSharedMemory(int sem_set, const volatile SharedMemory* shm, int fd);
