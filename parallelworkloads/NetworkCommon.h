#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

#define DISCOVER_PORT       "5161"
#define DISCOVER_PORT_V     5161
#define CALCULATE_PORT      "5162"
#define CALCULATE_PORT_V    5162

typedef struct {
    float l, r;
    size_t n;
} IntegrationRequest;

typedef struct {
    float ival;
} IntegrationResponse;

typedef int         Socket;
typedef const char* Port;

int sendAll(Socket sock, const void* buf, size_t* buf_sz_inout);
int recvAll(Socket sock,       void* buf, size_t* buf_sz_inout);

typedef int discovery_magic_t;
enum { DISCOVERY_MAGIC_V = 0x1DEAD1 };

#ifndef NETDEBUG
#define NETDEBUG 0
#endif

#if NETDEBUG
#include <stdio.h>
#define NetDebugPrint(...) fprintf(stderr, __VA_ARGS__)
#else
#define NetDebugPrint(...) (void)(__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif
