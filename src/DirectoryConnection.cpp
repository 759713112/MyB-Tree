#include "DirectoryConnection.h"

#include "Connection.h"

DirectoryConnection::DirectoryConnection(uint16_t dirID, void *dsmPool,
                                         uint64_t dsmSize, uint32_t machineNR,
                                         RemoteConnection *remoteInfo, RemoteConnection *dpuConnectInfo, bool isMemoryNode)
    : dirID(dirID), remoteInfo(remoteInfo), dpuConnectInfo(dpuConnectInfo) {

  createContext(&ctx);
  cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
  message = new RawMessageConnection(ctx, cq, DIR_MESSAGE_NR);

  message->initRecv();
  message->initSend();

  // dsm memory
  this->dsmPool = dsmPool;
  this->dsmSize = dsmSize;
  this->dsmMR = createMemoryRegion((uint64_t)dsmPool, dsmSize, &ctx);
  this->dsmLKey = dsmMR->lkey;

  // on-chip lock memory
  if (dirID == 0) {
    this->lockPool = (void *)define::kLockStartAddr;
    this->lockSize = define::kLockChipMemSize;
    this->lockMR = createMemoryRegionOnChip((uint64_t)this->lockPool,
                                            this->lockSize, &ctx);
    // 
    this->lockLKey = lockMR->lkey;
  }

  // app, RC
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    data2app[i] = new ibv_qp *[machineNR];
    for (size_t k = 0; k < machineNR; ++k) {
      createQueuePair(&data2app[i][k], IBV_QPT_RC, cq, &ctx);
    }
  }
  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    createQueuePair(&data2dpu[i], IBV_QPT_RC, cq, &ctx);
  }

}

void DirectoryConnection::sendMessage2App(RawMessage *m, uint16_t node_id,
                                          uint16_t th_id) {
  if (node_id == NODE_ID_FOR_DPU) {
    message->sendRawMessage(m, dpuConnectInfo[0].dpuMessageQPN[th_id],
                          dpuConnectInfo[0].dirToDpuAh[dirID][th_id]);
  } else {
    message->sendRawMessage(m, remoteInfo[node_id].appMessageQPN[th_id],
                          remoteInfo[node_id].dirToAppAh[dirID][th_id]);
  }
  
}
