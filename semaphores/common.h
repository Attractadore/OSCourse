#pragma once
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/types.h>

#ifdef DEBUG 
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while(0)
#endif

typedef struct {
    ssize_t size;
    enum { BUFFER_SIZE = 4096 - sizeof(ssize_t) };
    char buffer[BUFFER_SIZE];
} TransferBuffer;

typedef struct {
    TransferBuffer stub;
    TransferBuffer buffers[];
} SharedMemory;

SharedMemory* getSharedMemory();
int getBlockSemaphore(int i);
TransferBuffer* getBlockBuffer(SharedMemory* shm, int i);

int selectServerBlock();
int selectClientBlock();

void copyIntoSharedMemory(int sem_set, TransferBuffer* buffer, int fd);
void copyFromSharedMemory(int sem_set, const volatile TransferBuffer* buffer, int fd);
