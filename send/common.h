#pragma once
#include <stdio.h>

#define SYNC_FIFO_NAME "/tmp/sync_fifo"
#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(s, ...) do {} while(0)
#endif

int copyFile(int in, int out);
