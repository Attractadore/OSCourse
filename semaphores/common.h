#pragma once
#include <stdio.h>

typedef struct {
    enum { SHARED_MEMORY_SIZE = 4096 };
    unsigned size;
    char buffer[SHARED_MEMORY_SIZE];
} ShmBuffer;

#ifdef DEBUG 
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(s, ...) do {} while(0)
#endif

extern const char* ftok_name;
key_t getSemaphoreKey();
key_t getSharedMemoryKey();

enum {
    CLIENT_SEMAPHORE_INDEX = 0,
    SERVER_SEMAPHORE_INDEX = 1,
    SYNC_SEMAPHORE_INDEX = 2,
};

enum {
    SEMAPHORE_FTOK = 1,
    SHARED_MEMORY_FTOK = 2,
};
