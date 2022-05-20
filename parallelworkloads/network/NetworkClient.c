#define _GNU_SOURCE
#include "NetworkClient.h"

#include <arpa/inet.h>
#include <iso646.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    struct sockaddr_in* workers;
    size_t              worker_count;
} WorkerInfo;

static int GetWorkersForSocket(
    Socket s, unsigned short discover_port, WorkerInfo* pinfo
) {
    NetDebugPrint("Enable broadcast for socket %d\n", s);
    int broadcast = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))) {
        NetDebugPrint("Failed to enable broadcast\n");
        return -1;
    }

    struct sockaddr_in ai = {
        .sin_addr = {
            .s_addr = INADDR_BROADCAST,
        },
        .sin_family = AF_INET,
        .sin_port = htons(discover_port),
    };

    discovery_magic_t magic = DISCOVERY_MAGIC_V;
    ssize_t sz = sendto(s, &magic, sizeof(magic), 0, (const void*) &ai, sizeof(ai));
    if (sz != sizeof(magic)) {
        NetDebugPrint("Failed to broadcast discovery request\n");
        return -1;
    }

    static const long long total_timeout_ns =
        CLIENT_PEER_DISCOVERY_TIMEOUT_S * 1e9;
    long long timeout_ns = total_timeout_ns;

    struct timespec start, end;
    if (clock_gettime(CLOCK_REALTIME, &start)) {
        NetDebugPrint("Failed to set discovery start time\n");
        return -1;
    }

    while(timeout_ns > 0) {
        socklen_t addr_sz = sizeof(*pinfo->workers);
        void* new_workers = realloc(pinfo->workers, addr_sz * (pinfo->worker_count + 1));
        if (!new_workers) {
            NetDebugPrint("Allocation error\n");
            return -1;
        }
        pinfo->workers = new_workers;
        void* back = &pinfo->workers[pinfo->worker_count];

        struct timeval wait_time = {};
        wait_time.tv_sec = timeout_ns / (long long)1e9;
        wait_time.tv_usec = (timeout_ns - wait_time.tv_sec * 1e9) / 1e3;
        NetDebugPrint("Set discovery timeout to %lds %ldus\n", wait_time.tv_sec, wait_time.tv_usec);
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(wait_time))) {
            NetDebugPrint("Failed to set timeout\n");
            goto cont;
        }

        discovery_magic_t magic;
        NetDebugPrint("Recieve discovery broadcast response\n");
        size_t sz = recvfrom(s, &magic, sizeof(magic), 0, back, &addr_sz);
        if (sz != sizeof(magic)) {
            NetDebugPrint("Failed to recieve response\n");
            goto cont;
        } else if (magic != DISCOVERY_MAGIC_V) {
            NetDebugPrint("Wrong magic number\n");
            goto cont;
        }
        for (size_t i = 0; i < pinfo->worker_count; i++) {
            if (memcmp(&pinfo->workers[i], back, addr_sz) == 0) {
                NetDebugPrint("Duplicate\n");
                goto cont;
            }
        }
        NetDebugPrint("Recieved response\n");
        pinfo->worker_count++;

cont:
        if (clock_gettime(CLOCK_REALTIME, &end)) {
            NetDebugPrint("Failed to get current time\n");
            return -1;
        }
        long long delta_ns =
            (end.tv_sec - start.tv_sec) * 1e9 +
            end.tv_nsec - start.tv_nsec;
        timeout_ns -= delta_ns;
    }

    return 0;
}

static int GetWorkers(unsigned short discover_port, WorkerInfo* pinfo
) {
    Socket s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
        NetDebugPrint("Failed to create broadcast socket\n");
        return -1;
    }
    pinfo->workers = NULL;
    pinfo->worker_count = 0;
    int r = GetWorkersForSocket(s, discover_port, pinfo);
    close(s);
    if (r) {
        free(pinfo->workers);
    }
    return r;
}

static int SendAndRecieve(
    Socket s,
    const IntegrationRequest* ireq,
    IntegrationResponse* iresp_out
) {
    static const size_t ireq_sz = sizeof(*ireq);
    static const size_t iresp_sz = sizeof(*iresp_out);

    size_t send_sz = ireq_sz;
    NetDebugPrint("Send request\n");
    sendAll(s, ireq, &send_sz);
    NetDebugPrint("Sent %zu/%zu bytes\n", send_sz, ireq_sz);
    if (send_sz != ireq_sz) {
        return -1;
    }

    size_t rcv_sz = iresp_sz;
    NetDebugPrint("Receive response\n");
    recvAll(s, iresp_out, &rcv_sz);
    NetDebugPrint("Received %zu/%zu bytes\n", rcv_sz, iresp_sz);
    if (rcv_sz != iresp_sz) {
        return -1;
    }

    return 0;
}

typedef struct {
    Socket              s;
    IntegrationRequest  ireq;
    IntegrationResponse iresp;
    int                 r;
} ThreadData;

void* SendAndRecieveProxy(void* p) {
    ThreadData* thread_data = p;
    int r = SendAndRecieve(thread_data->s, &thread_data->ireq, &thread_data->iresp);
    return NULL;
}

static void GetResponsesImpl(
    size_t con_cnt,
    pthread_t* thread_ids,
    ThreadData* thread_datas
) {
    for (size_t i = 0; i < con_cnt; i++) {
        int r = pthread_create(&thread_ids[i], NULL, SendAndRecieveProxy, &thread_datas[i]);
        if (r) {
            thread_datas[i].r = r;
        }
    }
    for (size_t i = 0; i < con_cnt; i++) {
        pthread_t t = thread_ids[i];
        if (t) {
            pthread_join(t, NULL);
        }
    }
}

static int GetResponses(Socket* connections, size_t con_cnt, 
    const IntegrationRequest* ireq, IntegrationResponse* iresp_out
) {
    pthread_t* thread_ids = calloc(con_cnt, sizeof(*thread_ids));
    ThreadData* thread_datas = calloc(con_cnt, sizeof(*thread_datas));
    
    int ret = 0;
    if (thread_ids and thread_datas) {
        float step = (ireq->r - ireq->l) / con_cnt;
        float l = ireq->l;
        float r = l + step;
        size_t n = ireq->n / con_cnt;
        for (size_t i = 0; i < con_cnt; i++) {
            thread_datas[i].s    = connections[i];
            thread_datas[i].ireq = (IntegrationRequest) {
                .l = l,
                .r = r,
                .n = n,
            };
            l = r;
            r = l + step;
        }

        GetResponsesImpl(con_cnt, thread_ids, thread_datas);
        for (size_t i = 0; i < con_cnt and !r; i++) {
            ret = thread_datas[i].r;
        }

        if (!ret) {
            iresp_out->ival = 0.0f;
            for (size_t i = 0; i < con_cnt; i++) {
                iresp_out->ival += thread_datas[i].iresp.ival;
            }
        }
    } else {
        NetDebugPrint("Allocation error\n");
        ret = -1;
    }

    free(thread_ids);
    free(thread_datas);

    return ret;
}

int ClientSend(const IntegrationRequest* ireq, IntegrationResponse* iresp_out) {
    WorkerInfo pinfo;
    if (GetWorkers(DISCOVER_PORT_V, &pinfo)) {
        return -1;
    } else if (!pinfo.worker_count) {
        free(pinfo.workers);
        NetDebugPrint("Failed to find any workers\n");
        return -1;
    }

    for (size_t i = 0; i < pinfo.worker_count; i++) {
        pinfo.workers[i].sin_port = htons(CALCULATE_PORT_V);
    }

    for (size_t i = 0; i < pinfo.worker_count; i++) {
        char buf[16];
        inet_ntop(AF_INET, &pinfo.workers[i].sin_addr, buf, sizeof(buf));
        NetDebugPrint("Found worker %zu %s\n", i, buf);
    }

    Socket* connections = calloc(pinfo.worker_count, sizeof(*connections));
    if (!connections) {
        free(pinfo.workers);
        NetDebugPrint("Allocation error\n");
        return -1;
    }
    size_t con_cnt = 0;
    for (size_t i = 0; i < pinfo.worker_count; i++) {
        Socket s = socket(PF_INET, SOCK_STREAM, 0);
        if (connect(s, (const void*) &pinfo.workers[i], sizeof(pinfo.workers[i]))) {
            NetDebugPrint("Failed to connect to socket %d\n", s);
            close(s);
            continue;
        }
        struct timeval timeout = {
            .tv_sec = CLIENT_PEER_RECIEVE_TIMEOUT_S,
        };
        NetDebugPrint("Set %lds timeout for socket %d\n", timeout.tv_sec, s);
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
            NetDebugPrint("Failed to set timeout\n");
            close(s);
            continue;
        }
        connections[con_cnt++] = s;
        NetDebugPrint("Connected to socket %d\n", s);
    }
    free(pinfo.workers);

    if (!con_cnt) {
        free(connections);
        NetDebugPrint("Failed establish any worker connections\n");
        return -1;
    }

    int r = GetResponses(connections, con_cnt, ireq, iresp_out);

    for (size_t i = 0; i < con_cnt; i++) {
        close(connections[i]);
    }
    free(connections);

    return r;
}
