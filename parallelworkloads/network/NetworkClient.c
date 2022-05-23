#define _GNU_SOURCE
#include "NetworkClient.h"

#include <arpa/inet.h>
#include <iso646.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    struct sockaddr_in  addr;
    WorkerProperties    props;
} WorkerInfo;

typedef struct {
    WorkerInfo* workers;
    size_t      worker_count;
} WorkersInfo;

typedef struct {
    Socket              socket;
    WorkerProperties    props;
} WorkerConnection;

static int GetWorkersForSocket(
    Socket s, unsigned short discover_port, WorkersInfo* pinfo
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

    DiscoveryRequestPacket dreq = {
        .magic = MAGIC_V,
    };
    ssize_t sz = sendto(s, &dreq, sizeof(dreq), 0, (const void*) &ai, sizeof(ai));
    if (sz != sizeof(dreq)) {
        NetDebugPrint("Failed to broadcast discovery request\n");
        return -1;
    }

    static const long long total_timeout_ns =
        CLIENT_PEER_DISCOVERY_TIMEOUT_S * 1e9;
    long long timeout_ns = total_timeout_ns;
    NetDebugPrint("Total discovery time: %.3fs\n", total_timeout_ns / 1e9);

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
        WorkerInfo* back = &pinfo->workers[pinfo->worker_count];

        struct timeval wait_time = {};
        wait_time.tv_sec = timeout_ns / (long long)1e9;
        wait_time.tv_usec = (timeout_ns - wait_time.tv_sec * (long long)1e9) / (long long) 1e3;
        NetDebugPrint("Set discovery timeout to %lds %ldus\n", wait_time.tv_sec, wait_time.tv_usec);
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(wait_time))) {
            NetDebugPrint("Failed to set timeout\n");
            goto cont;
        }

        DiscoveryResponsePacket dresp;
        NetDebugPrint("Recieve discovery broadcast response\n");
        ssize_t sz = recvfrom(s, &dresp, sizeof(dresp), 0, &back->addr, &addr_sz);
        if (sz != sizeof(dresp)) {
            NetDebugPrint("Failed to recieve response\n");
            goto cont;
        } else if (dresp.magic != MAGIC_V) {
            NetDebugPrint("Wrong magic number\n");
            goto cont;
        }
        back->props = dresp.response.props;
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
            (end.tv_sec - start.tv_sec) * (long long)1e9 +
            end.tv_nsec - start.tv_nsec;
        timeout_ns -= delta_ns;
    }

    return 0;
}

static int GetWorkers(unsigned short discover_port, WorkersInfo* pinfo
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
    IntegrationRequestPacket ireqp = {
        .magic = MAGIC_V,
        .request = *ireq,
    };
    size_t send_sz = sizeof(ireqp);
    NetDebugPrint("Send request to socket %d\n", s);
    sendAll(s, &ireqp, &send_sz);
    NetDebugPrint("Sent %zu/%zu bytes to socket %d\n", send_sz, sizeof(ireqp), s);
    if (send_sz != sizeof(ireqp)) {
        return -1;
    }

    IntegrationResponsePacket irespp;
    size_t rcv_sz = sizeof(irespp);
    NetDebugPrint("Receive response from socket %d\n", s);
    recvAll(s, &irespp, &rcv_sz);
    NetDebugPrint("Received %zu/%zu bytes from socket %d\n", rcv_sz, sizeof(irespp), s);
    if (rcv_sz != sizeof(irespp)) {
        return -1;
    } else if (irespp.magic != MAGIC_V) {
        NetDebugPrint("Wrong magic number for socket %d\n", s);
        return -1;
    }
    *iresp_out = irespp.response;

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
    thread_data->r = SendAndRecieve(thread_data->s, &thread_data->ireq, &thread_data->iresp);
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

static int GetResponses(WorkerConnection* connections, size_t con_cnt,
    const IntegrationRequest* ireq, IntegrationResponse* iresp_out
) {
    pthread_t* thread_ids = calloc(con_cnt, sizeof(*thread_ids));
    ThreadData* thread_datas = calloc(con_cnt, sizeof(*thread_datas));
    
    int ret = 0;
    if (thread_ids and thread_datas) {
        double total_throughput = 0;
        for (size_t i = 0; i < con_cnt; i++) {
            total_throughput += connections[i].props.throughput;
        }

        size_t n = ireq->n;
        size_t used_n = 0;
        float step = (ireq->r - ireq->l) / n;
        float l = ireq->l;
        for (size_t i = 0; i < con_cnt - 1; i++) {
            size_t this_n = connections[i].props.throughput / total_throughput * n;
            used_n += this_n;
            float r = l + this_n * step;
            thread_datas[i].s    = connections[i].socket;
            thread_datas[i].ireq = (IntegrationRequest) {
                .l = l,
                .r = r,
                .n = this_n,
            };
            l = r;
        }
        // Schedule last piece
        {
            size_t i = con_cnt - 1;
            size_t this_n = n - used_n;
            float r = ireq->r;
            thread_datas[i].s    = connections[i].socket;
            thread_datas[i].ireq = (IntegrationRequest) {
                .l = l,
                .r = r,
                .n = this_n,
            };
        }
        for (size_t i = 0; i < con_cnt; i++) {
            const IntegrationRequest* ir = &thread_datas[i].ireq;
            NetDebugPrint("Schedule request [%f; %f]/%zu to socket %d\n", ir->l, ir->r, ir->n, connections[i].socket);
        }

        GetResponsesImpl(con_cnt, thread_ids, thread_datas);
        for (size_t i = 0; i < con_cnt and !ret; i++) {
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
    WorkersInfo pinfo;
    if (GetWorkers(DISCOVER_PORT_V, &pinfo)) {
        return -1;
    } else if (!pinfo.worker_count) {
        free(pinfo.workers);
        NetDebugPrint("Failed to find any workers\n");
        return -1;
    }

    for (size_t i = 0; i < pinfo.worker_count; i++) {
        pinfo.workers[i].addr.sin_port = htons(CALCULATE_PORT_V);
    }

    for (size_t i = 0; i < pinfo.worker_count; i++) {
        char buf[16];
        inet_ntop(AF_INET, &pinfo.workers[i].addr.sin_addr, buf, sizeof(buf));
        NetDebugPrint("Found worker %zu at %s with %u threads and throughput %.0f/s\n",
            i, buf, pinfo.workers[i].props.thread_count, pinfo.workers[i].props.throughput);
    }

    WorkerConnection* connections = calloc(pinfo.worker_count, sizeof(*connections));
    if (!connections) {
        free(pinfo.workers);
        NetDebugPrint("Allocation error\n");
        return -1;
    }
    size_t con_cnt = 0;
    for (size_t i = 0; i < pinfo.worker_count; i++) {
        Socket s = socket(PF_INET, SOCK_STREAM, 0);
        if (connect(s, &pinfo.workers[i].addr, sizeof(pinfo.workers[i].addr))) {
            NetDebugPrint("Failed to connect to socket %d\n", s);
            close(s);
            continue;
        }
        struct timeval timeout = {
            .tv_sec = CLIENT_PEER_RECIEVE_TIMEOUT_S,
            .tv_usec = (CLIENT_PEER_RECIEVE_TIMEOUT_S - timeout.tv_sec) * 1e6,
        };
        NetDebugPrint("Set %lds %ldus timeout for socket %d\n", timeout.tv_sec, timeout.tv_usec, s);
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
            NetDebugPrint("Failed to set timeout\n");
            close(s);
            continue;
        }
        connections[con_cnt++] = (WorkerConnection) {
            .socket = s,
            .props  = pinfo.workers[i].props,
        };
        NetDebugPrint("Connected to socket %d for worker %zu\n", s, i);
    }
    free(pinfo.workers);

    int r = 0;
    if (con_cnt) {
        r = GetResponses(connections, con_cnt, ireq, iresp_out);
    } else {
        NetDebugPrint("Failed establish any worker connections\n");
        r = -1;
    }
    for (size_t i = 0; i < con_cnt; i++) {
        close(connections[i].socket);
    }
    free(connections);
    return r;
}
