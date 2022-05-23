#define _GNU_SOURCE
#include "NetworkServer.h"

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <poll.h>

static int AcquireFlock(const char* path) {
    int f = open(path, O_CREAT, 0600);
    if (flock(f, LOCK_EX | LOCK_NB)) {
        close(f);
        return -1;
    }
    return f;
}

int static ServerListenImpl(unsigned short listen_port, ListenInfo* linfo) {
    static const char* lock_file_path = "/tmp/pw_lock_file";

    int l = linfo->lock = AcquireFlock(lock_file_path);
    if (l == -1) {
        NetDebugPrint("Failed to acquire lock file %s\n", lock_file_path);
        return -1;
    }

    struct sockaddr_in ai = {
        .sin_addr = {
            .s_addr = INADDR_ANY,
        },
        .sin_family = AF_INET,
        .sin_port = htons(listen_port),
    };

    Socket s = linfo->socket = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        return -1;
    }
    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        return -1;
    }
    if (bind(s, (const void*) &ai, sizeof(ai))) {
        return -1;
    }
    enum { QUEUE_COUNT = 10 };
    if (listen(s, QUEUE_COUNT)) {
        return -1;
    }

    NetDebugPrint("Begin listening on socket %d\n", s);

    return 0;

}

int ServerListen(unsigned short listen_port, ListenInfo* linfo) {
    *linfo = (ListenInfo) {
        .lock = -1,
        .socket = -1,
    };
    int r = ServerListenImpl(listen_port, linfo);
    if (r) {
        close(linfo->lock);
        close(linfo->socket);
    }
    return r;
}

static int ServerRecieveImpl(const ListenInfo* linfo, RequestInfo* rinfo) {
    NetDebugPrint("Accept connection on socket %d\n", linfo->socket);
    Socket s = rinfo->socket = accept(linfo->socket, NULL, NULL);
    if (s == -1) {
        NetDebugPrint("Failed to accept connection\n");
        return 1;
    }
    NetDebugPrint("Accepted connection %d\n", rinfo->socket);

    struct timeval timeout = {
        .tv_sec = SERVER_RECIEVE_TIMEOUT_S,
        .tv_usec = (SERVER_RECIEVE_TIMEOUT_S - timeout.tv_sec) * 1e6,
    };
    NetDebugPrint("Set %lds timeout for socket %d\n", timeout.tv_sec, s);
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        NetDebugPrint("Failed to set timeout\n");
        return 1;
    }

    IntegrationRequestPacket ireqp;
    size_t rcv_sz = sizeof(ireqp);
    NetDebugPrint("Receive request\n");
    recvAll(s, &ireqp, &rcv_sz);
    NetDebugPrint("Received %zu/%zu bytes\n", rcv_sz, sizeof(ireqp));
    if (rcv_sz != sizeof(ireqp)) {
        return 1;
    } else if (ireqp.magic != MAGIC_V) {
        NetDebugPrint("Wrong magic number\n");
        return 1;
    }
    rinfo->integration_request = ireqp.request;

    {   const IntegrationRequest* ir = &rinfo->integration_request;
        NetDebugPrint("Received request [%f; %f]/%zu\n", ir->l, ir->r, ir->n);  }

    return 0;
}

int ServerRecieve(const ListenInfo* linfo, RequestInfo* rinfo) {
    rinfo->socket = -1;
    int r = ServerRecieveImpl(linfo, rinfo);
    if (r) {
        close(rinfo->socket);
    }
    return r;
}

int ServerRespond(const ResponseInfo* rinfo) {
    IntegrationResponsePacket irespp = {
        .magic = MAGIC_V,
        .response = rinfo->integration_response,
    };
    size_t send_sz = sizeof(irespp);
    NetDebugPrint("Send response\n");
    sendAll(rinfo->socket, &irespp, &send_sz);
    NetDebugPrint("Sent %zu/%zu bytes\n", send_sz, sizeof(irespp));
    close(rinfo->socket);
    if (send_sz != sizeof(irespp)) {
        return 1;
    }
    return 0;
}

void ServerRespondError(const RequestInfo* rinfo) {
    close(rinfo->socket);
}

static int ServerStartDiscoveryImpl(
    unsigned short discover_port, DiscoveryInfo* dinfo
) {
    struct sockaddr_in ai = {
        .sin_addr = {
            .s_addr = INADDR_ANY,
        },
        .sin_family = AF_INET,
        .sin_port = htons(discover_port),
    };

    Socket s = dinfo->socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
        return -1;
    }
    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        return -1;
    }
    if (bind(s, (const void*)&ai, sizeof(ai))) {
        return -1;
    }

    NetDebugPrint("Begin discovery service on socket %d\n", s);

    return 0;
}

int ServerStartDiscovery(unsigned short discover_port, DiscoveryInfo* dinfo) {
    dinfo->socket = -1;
    int r = ServerStartDiscoveryImpl(discover_port, dinfo);
    if (r) {
        close(dinfo->socket);
    }
    return r;
}

int ServerProcessDiscoveryRequest(const DiscoveryInfo* dinfo) {
    struct sockaddr_storage they;
    socklen_t they_sz = sizeof(they);

    DiscoveryRequestPacket dreqp;
    NetDebugPrint("Wait for discovery request on socket %d\n", dinfo->socket);
    ssize_t sz = recvfrom(dinfo->socket, &dreqp, sizeof(dreqp), 0, (void*) &they, &they_sz);
    if (sz != sizeof(dreqp)) {
        NetDebugPrint("Failed to recieve discovery request\n");
        return 1;
    } else if (dreqp.magic != MAGIC_V) {
        NetDebugPrint("Wrong magic number\n");
        return 1;
    }

    DiscoveryResponsePacket drespp = {
        .magic = MAGIC_V,
        .response = dinfo->response,
    };
    NetDebugPrint("Send discovery response\n");
    sz = sendto(dinfo->socket, &drespp, sizeof(drespp), 0, (const void*) &they, they_sz);
    if (sz != sizeof(drespp)) {
        NetDebugPrint("Failed to send discovery response\n");
        return 1;
    }

    return 0;
}
