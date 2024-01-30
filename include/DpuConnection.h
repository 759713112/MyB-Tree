#ifndef __DPU_CONNECTION_H__
#define __DPU_CONNECTION_H__

#include "AbstractMessageConnection.h"
#include "GlobalAddress.h"

#include <thread>

enum DpuRequestType : uint8_t {
  INSERT,
  SEARCH,
  DELETE,
  SCAN,
};

struct DpuRequest {
  DpuRequestType type;
  uint16_t node_id;
  uint16_t app_id;
  uint16_t coro_id;
  uint16_t wr_id;
  Key k;
} __attribute__((packed));

struct DpuResponse {
  uint16_t coro_id;
  char buffer[36];
} __attribute__((packed));

class DpuConnection {
  const static int kBatchCount = 4;
public:
  DpuConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR, uint64_t sendSize, 
                  uint64_t recvSize, uint64_t wr_id = 0);
  void sendDpuRequest(DpuRequest *m);
  void sendDpuResponse(DpuResponse *m);
  void initQPtoRTS(uint32_t remoteQPN, uint16_t remoteLid, uint8_t *gid);

  void initRecv();
  DpuResponse* getResponse();
  DpuRequest* getRequest();
  char *getMessage();
  char *getSendPool();
  uint32_t getQPN() { return message->qp_num; }
private:
  RdmaContext &ctx;
  const uint64_t sendSize;
  const uint64_t recvSize;
  ibv_qp *message; 
  uint16_t messageNR;


  ibv_mr *messageMR;
  void *messagePool;
  uint32_t messageLkey;

  uint16_t curMessage;

  void *sendPool;
  uint16_t curSend;

  ibv_recv_wr *recvs[kBatchCount];
  ibv_sge *recv_sgl[kBatchCount];
  uint32_t subNR;

  ibv_cq *send_cq;
  uint64_t sendCounter;

  ibv_cq *cq;
  uint64_t wr_id;
};

#endif /* __DPU_CONNECTION_H__ */
