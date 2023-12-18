#include <iostream>
#include <thread>

#include "DpuProxy.h"

thread_local int DpuProxy::thread_id = -1;


DpuProxy::DpuProxy(uint32_t thread_num, RemoteConnection *remoteInfo) {
  
  createContext(&this->ctx);

  this->complete_queues.resize(DPU_CONNECTION_NUMS);
  this->dpuProxConnections.resize(DPU_CONNECTION_NUMS);
  for (auto &con: dpuProxConnections) {
    ibv_cq *cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
    con = new DpuProxyConnection(ctx, cq, DIR_MESSAGE_NR);
    con->initRecv();
  }

  this->threads.resize(thread_num);
  for (uint32_t i = 0; i < thread_num; i++) {
    threads[i] = std::thread(&DpuProxy::run, this, i);
  }

}

DpuProxy::~DpuProxy() {

}


void DpuProxy::run(int thread_id) {
  this->thread_id = thread_id;
  bindCore(thread_id % 16);

  while (true) {
    struct ibv_wc wc;
    auto conn = this->dpuProxConnections[thread_id % DPU_CONNECTION_NUMS];
    pollWithCQ(conn->get_cq(), 1, &wc);

    switch (int(wc.opcode)) {
    case IBV_WC_RECV: // control message
    {

      auto *m = (Request *)conn->getMessage();

      process_request(m);

      break;
    }
    case IBV_WC_RDMA_WRITE: {
      break;
    }
    case IBV_WC_RECV_RDMA_WITH_IMM: {

      break;
    }
    default:
      assert(false);
    }
  }
}

void DpuProxy::process_request(const Request *req) {
    std::cout << "process_request" << std::endl;
    sleep(1);
}