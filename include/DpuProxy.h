#ifndef __DPU_PROXY_H__
#define __DPU_PROXY_H__

#define DPU_CONNECTION_NUMS 16

#include <vector>
#include <thread>

#include "GlobalAddress.h"
#include "Connection.h"
#include "DpuProxyConnection.h"

class Cache {

};



class DpuProxy {
public:
    DpuProxy(uint32_t thread_num, RemoteConnection *remoteInfo);
    ~DpuProxy();
    void run(int thread_id);

private:
    void process_request(const Request *req);

private:    
    std::vector<std::thread> threads;
    
    std::vector<ibv_cq*> complete_queues;
    std::vector<DpuProxyConnection*> dpuProxConnections;  
    Cache cache;
    uint32_t machineNR;
    RemoteConnection *remoteInfo;
    RdmaContext ctx;
    ibv_cq *cq;

    static thread_local int thread_id;

};


#endif