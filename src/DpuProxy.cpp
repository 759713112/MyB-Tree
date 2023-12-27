
#include "DpuProxy.h"
#include "Directory.h"
#include "HugePageAlloc.h"

#include "DSDpuKeeper.h"

#include <algorithm>

// thread_local int DpuProxy::thread_id = -1;
// thread_local ThreadConnection *DpuProxy::iCon = nullptr;
// thread_local char *DpuProxy::rdma_buffer = nullptr;
// thread_local LocalAllocator DpuProxy::local_allocator;
// thread_local RdmaBuffer DpuProxy::rbuf[define::kMaxCoro];
// thread_local uint64_t DpuProxy::thread_tag = 0;

DpuProxy *DpuProxy::getInstance(const DSMConfig &conf) {
  static DpuProxy dsm(conf);
  return &dsm;
}

DpuProxy::DpuProxy(const DSMConfig &conf)
    : DSM(conf) {

  remoteInfo = new RemoteConnection[0];
  computeInfo = new RemoteConnection[conf.machineNR];

  
  Debug::notifyInfo("number of servers (colocated MN/CN): %d", conf.machineNR);
  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    thCon[i] =  new ThreadConnection(i, (void *)cache.data, cache.size * define::GB, 1, remoteInfo);
    // auto rpc_cq = ibv_create_cq(hostCon[i]->ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
    // dpuCon[i] = new DpuConnection(hostCon[i]->ctx, rpc_cq, APP_MESSAGE_NR);
    // dpuCon[i]->initRecv();
  }

  
  keeper = new DSDpuKeeper(thCon, remoteInfo, computeInfo, conf.machineNR);
  myNodeID = keeper->getMyNodeID();
  keeper->barrier("DpuProxy-init");
}

DpuProxy::~DpuProxy() {}

// void DpuProxy::registerThread() {

//   static bool has_init[MAX_APP_THREAD];

//   if (thread_id != -1)
//     return;

//   thread_id = appID.fetch_add(1);
//   thread_tag = thread_id + (((uint64_t)this->getMyNodeID()) << 32) + 1;

//   iCon = hostCon[thread_id];

//   if (!has_init[thread_id]) {
//     has_init[thread_id] = true;
//   }

//   rdma_buffer = (char *)cache.data + thread_id * 12 * define::MB;

//   for (int i = 0; i < define::kMaxCoro; ++i) {
//     rbuf[i].set_buffer(rdma_buffer + i * define::kPerCoroRdmaBuf);
//   }
// }

void DpuProxy::rpc_call_dir(const RawMessage &m, uint16_t node_id,
                    uint16_t dir_id = 0) {

    auto buffer = (RawMessage *)iCon->message->getSendPool();

    memcpy(buffer, &m, sizeof(RawMessage));
    buffer->node_id = myNodeID;
    buffer->app_id = thread_id;

    iCon->sendMessage2Dir(buffer, node_id, dir_id);
}