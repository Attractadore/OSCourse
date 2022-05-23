#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "NetworkCommon.h"

static double SERVER_RECIEVE_TIMEOUT_S = 1;

typedef struct {
    Socket  socket;
    int     lock;
} ListenInfo;

int ServerListen(unsigned short listen_port, ListenInfo* linfo_out);

typedef struct {
    IntegrationRequest integration_request;
    Socket socket;
} RequestInfo;

int ServerRecieve(const ListenInfo* linfo, RequestInfo* rinfo_out);

typedef struct {
    IntegrationResponse integration_response;
    Socket socket;
} ResponseInfo;

int ServerRespond(const ResponseInfo* rinfo);

void ServerRespondError(const RequestInfo* rinfo);

typedef struct {
    Socket socket;
    DiscoveryResponse response;
} DiscoveryInfo;

int ServerStartDiscovery(unsigned short discovery_port, DiscoveryInfo* dinfo_out);

int ServerProcessDiscoveryRequest(const DiscoveryInfo* dinfo);

#ifdef __cplusplus
}
#endif
