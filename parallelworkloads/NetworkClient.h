#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "NetworkCommon.h"

enum {
    CLIENT_PEER_DISCOVERY_TIMEOUT_S = 1,
    CLIENT_PEER_RECIEVE_TIMEOUT_S   = 60,
};

int ClientSend(const IntegrationRequest* ireq, IntegrationResponse* iresp);

#ifdef __cplusplus
}
#endif
