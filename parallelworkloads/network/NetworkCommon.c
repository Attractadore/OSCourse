#include "NetworkCommon.h"

#include <iso646.h>
#include <stdbool.h>
#include <sys/socket.h>

int sendAll(Socket sock, const void* buf, size_t* buf_sz_inout) {
    size_t buf_sz = *buf_sz_inout;
    size_t send_sz = 0;
    size_t sz = 0;
    while (send_sz < buf_sz) {
        sz = send(sock, (const char*)buf + send_sz, buf_sz - send_sz, 0);
        if (sz > 0) {
            send_sz += sz;
        } else {
            break;
        }
    }
    *buf_sz_inout = send_sz;
    return sz;
}

int recvAll(Socket sock, void* buf, size_t* buf_sz_inout) {
    size_t buf_sz = *buf_sz_inout;
    size_t rcv_sz = 0;
    ssize_t sz = 0;
    while (rcv_sz < buf_sz) {
        sz = recv(sock, (char*)buf + rcv_sz, buf_sz - rcv_sz, 0);
        if (sz > 0) {
            rcv_sz += sz;
        } else {
            break;
        }
    }
    *buf_sz_inout = rcv_sz;
    return sz;
}
