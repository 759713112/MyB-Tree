#include "DpuConnection.h"
#include "Rdma.h"

#include <cassert>

DpuConnection::DpuConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR, uint64_t sendSize, 
                  uint64_t recvSize, uint64_t wr_id)
    : ctx(ctx), messageNR(messageNR), sendSize(sendSize), recvSize(recvSize), cq(cq), wr_id(wr_id) {
  
  assert(messageNR % kBatchCount == 0);

  send_cq = ibv_create_cq(ctx.ctx, 128, NULL, NULL, 0);
  createQueuePair(&message, IBV_QPT_RC, send_cq, cq, &ctx);
  
  messagePool = hugePageAlloc(messageNR * (sendSize + recvSize));
  messageMR = createMemoryRegion((uint64_t)messagePool,
                                 messageNR * (sendSize + recvSize), &ctx);
  sendPool = (char *)messagePool + messageNR * recvSize;
  messageLkey = messageMR->lkey;
}


void DpuConnection::sendDpuRequest(DpuRequest *m) {

  if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
    ibv_wc wc;
    pollWithCQ(send_cq, 1, &wc);
  }

  rdmaSend(message, (uint64_t)m, sizeof(DpuRequest),
           messageLkey, (sendCounter & SIGNAL_BATCH) == 0);

  ++sendCounter;
}

void DpuConnection::sendDpuResponse(DpuResponse *m) {

  if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
    ibv_wc wc;
    pollWithCQ(send_cq, 1, &wc);
  }

  rdmaSend(message, (uint64_t)m, sizeof(DpuResponse),
           messageLkey, (sendCounter & SIGNAL_BATCH) == 0);

  ++sendCounter;
}

void DpuConnection::initRecv() { 
  subNR = messageNR / kBatchCount;

  for (int i = 0; i < kBatchCount; ++i) {
    recvs[i] = new ibv_recv_wr[subNR];
    recv_sgl[i] = new ibv_sge[subNR];
  }

  for (int k = 0; k < kBatchCount; ++k) {
    for (size_t i = 0; i < subNR; ++i) {
      auto &s = recv_sgl[k][i];
      memset(&s, 0, sizeof(s));

      s.addr = (uint64_t)messagePool + (k * subNR + i) * recvSize;
      s.length = recvSize;
      s.lkey = messageLkey;

      auto &r = recvs[k][i];
      memset(&r, 0, sizeof(r));

      r.wr_id = wr_id;
      r.sg_list = &s;
      r.num_sge = 1;
      r.next = (i == subNR - 1) ? NULL : &recvs[k][i + 1];
    }
  }

  struct ibv_recv_wr *bad;
  for (int i = 0; i < kBatchCount; ++i) {
    if (ibv_post_recv(message, &recvs[i][0], &bad)) {
      Debug::notifyError("Post receive failed.");
    }
  }
}

void DpuConnection::initQPtoRTS(uint32_t remoteQPN, uint16_t remoteLid, uint8_t *gid) {

  if (!modifyQPtoInit(message, &ctx)) {
    return;
  }

  if (!modifyQPtoRTR(message, remoteQPN, remoteLid, gid, &ctx)) {
    return;
  }

  if (!modifyQPtoRTS(message)) {
    return;
  }

}

DpuRequest* DpuConnection::getRequest() {
  return (DpuRequest*)this->getMessage();
}

DpuResponse* DpuConnection::getResponse() {
  return (DpuResponse*)this->getMessage();
}

char* DpuConnection::getMessage() {
  struct ibv_recv_wr *bad;
  char *m = (char *)messagePool + curMessage * recvSize;

  ADD_ROUND(curMessage, messageNR);

  if (curMessage % subNR == 0) {
    if (ibv_post_recv(
            message,
            &recvs[(curMessage / subNR - 1 + kBatchCount) % kBatchCount][0],
            &bad)) {
      Debug::notifyError("Post receive failed.");
    }
  }

  return m;

}

char* DpuConnection::getSendPool() {
  char *s = (char *)sendPool + curSend * sendSize;

  ADD_ROUND(curSend, messageNR);

  return s;
}


