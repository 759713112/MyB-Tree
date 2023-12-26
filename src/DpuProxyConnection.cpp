#include "DpuProxyConnection.h"

DpuProxyConnection::DpuProxyConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR)
      : AbstractMessageConnection(IBV_QPT_UD, 0, 40, ctx, cq, messageNR) {
    this->cq = cq;
}

void DpuProxyConnection::initSend() {

}

void DpuProxyConnection::sendRawMessage(Response *rsp, uint32_t remoteQPN, ibv_ah *ah) {

    if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
        ibv_wc wc;
        pollWithCQ(send_cq, 1, &wc);
    }

    rdmaSend(message, (uint64_t)rsp - sendPadding, sizeof(Response) + sendPadding,
            messageLkey, ah, remoteQPN, (sendCounter & SIGNAL_BATCH) == 0);

    ++sendCounter;
}