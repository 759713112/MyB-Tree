#ifndef __DPU_PROXY_CONNECTION_H__
#define __DPU_PROXY_CONNECTION_H__

#include <vector>
#include <thread>

#include "GlobalAddress.h"
#include "AbstractMessageConnection.h"

struct Request {
    enum RequestType {
        SEARCH,
        INSERT, 
    } requestType;
    uint16_t node_id;
    uint16_t app_id;
    uint64_t key;
    GlobalAddress addr;

}__attribute__((packed));


struct Response {
    enum ReponseType {
        SEARCH_RESPONSE,
        INSERT_RESPONSE, 
    } requestType;
    void *value;
    uint64_t *key;
    std::vector<GlobalAddress> path;

}__attribute__((packed));

class DpuProxyConnection : public AbstractMessageConnection {

public:
    DpuProxyConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR);

    ibv_cq* get_cq() { return this->cq; }
    void initSend();
    void sendRawMessage(Response *rsp, uint32_t remoteQPN, ibv_ah *ah);
private:
    ibv_cq *cq;
};

#endif