#include "ThreadConnection.h"

#include "Connection.h"

#include <iostream>
ThreadConnection::ThreadConnection(uint16_t threadID, void *cachePool,
                                   uint64_t cacheSize, uint32_t machineNR,
                                   RemoteConnection *remoteInfo)
    : threadID(threadID), remoteInfo(remoteInfo) {
  createContext(&ctx);

  cq2dpu = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
  dpuConnect = new DpuConnection(ctx, cq2dpu, APP_MESSAGE_NR, 1024, 1024);

  this->cachePool = cachePool;
#ifndef AARCH64

  // rpc_cq = cq;
  rpc_cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
  message = new RawMessageConnection(ctx, rpc_cq, APP_MESSAGE_NR);
  message->initRecv();
  cacheMR = createMemoryRegion((uint64_t)cachePool, cacheSize, &ctx);
  cacheLKey = cacheMR->lkey;


#endif
  cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
  // dir, RC
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    data[i] = new ibv_qp *[machineNR];
    for (size_t k = 0; k < machineNR; ++k) {
      ibv_qp *a;
      createQueuePair(&data[i][k], IBV_QPT_RC, cq, &ctx);
      createQueuePair(&a, IBV_QPT_RC, cq, &ctx);
    }
  }
}

void ThreadConnection::sendMessage2Dir(RawMessage *m, uint16_t node_id,
                                       uint16_t dir_id) {
  message->sendRawMessage(m, remoteInfo[node_id].dirMessageQPN[dir_id],
                          remoteInfo[node_id].appToDirAh[threadID][dir_id]);
  // // 需要改
  // exit(-2);
}
