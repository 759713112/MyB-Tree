#include "DpuConnection.h"

#include <cassert>

DpuConnection::DpuConnection(RdmaContext &ctx, ibv_cq *cq,
                                           uint32_t messageNR)
    : AbstractMessageConnection(IBV_QPT_UD, 0, 40, ctx, cq, messageNR), cq(cq) {
      
}

void DpuConnection::initSend() {}

void DpuConnection::sendDpuRequest(DpuRequest *m, uint32_t remoteQPN,
                                          ibv_ah *ah) {

  if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
    ibv_wc wc;
    pollWithCQ(send_cq, 1, &wc);
  }

  rdmaSend(message, (uint64_t)m - sendPadding, sizeof(DpuRequest) + sendPadding,
           messageLkey, ah, remoteQPN, (sendCounter & SIGNAL_BATCH) == 0);

  ++sendCounter;
}

void DpuConnection::sendDpuResponse(DpuResponse *m, uint32_t remoteQPN,
                                          ibv_ah *ah) {

  if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
    ibv_wc wc;
    pollWithCQ(send_cq, 1, &wc);
  }

  rdmaSend(message, (uint64_t)m - sendPadding, sizeof(DpuResponse ) + sendPadding,
           messageLkey, ah, remoteQPN, (sendCounter & SIGNAL_BATCH) == 0);

  ++sendCounter;
}
