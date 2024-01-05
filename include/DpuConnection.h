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

  Key k;
  Value v; // for malloc
} __attribute__((packed));

struct DpuResponse {
  int level;
} __attribute__((packed));

class DpuConnection : public AbstractMessageConnection {

public:
  DpuConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR);

  void initSend();
  void sendDpuRequest(DpuRequest *m, uint32_t remoteQPN, ibv_ah *ah);
  void sendDpuResponse(DpuResponse *m, uint32_t remoteQPN, ibv_ah *ah);
  ibv_cq* get_cq() { return this->cq; }
private:
  ibv_cq *cq;
};

#endif /* __DPU_CONNECTION_H__ */
