#ifndef __DPU_PROXY_H__
#define __DPU_PROXY_H__

#include <atomic>

#include "Cache.h"
#include "Config.h"
#include "Connection.h"
#include "DSMKeeper.h"
#include "GlobalAddress.h"
#include "LocalAllocator.h"
#include "RdmaBuffer.h"
#include "DpuConnection.h"
#include "DSDpuKeeper.h"
#include "DSM.h"
#include "DmaDpu.h"
#include "DpuCache.h"

class DSMKeeper;
class Directory;

class DpuProxy: public DSM {

public:
  // obtain netowrk resources for a thread
  // virtual void registerThread() override;
  // clear the network resources for all threads
  void resetThread() { appID.store(0); }
  virtual void registerThread() override; 
  static DpuProxy *getInstance(const DSMConfig &conf);
  virtual void rpc_call_dir(const RawMessage &m, uint16_t node_id,
                    uint16_t dir_id = 0) override;

  virtual void read(char *buffer, GlobalAddress gaddr, size_t size, bool signal,
              CoroContext *ctx) override;
  void read_sync(char *buffer, GlobalAddress gaddr, size_t size,
                    CoroContext *ctx);
  
  void* readByDmaFixedSize(GlobalAddress gaddr, CoroContext *ctx);

  bool poll_dma_cq_once(uint64_t &next_coro_id);

  int pollRpcDpu(std::queue<DpuRequest*> &task_queue, int poll_number) {
    static thread_local ibv_wc wc[8];
    poll_number = poll_number > 8 ? 8 : poll_number;
    int success_count = pollOnce(completeQueue, poll_number, wc);
    if (success_count <= 0) return 0;
    else {
      for (int i = 0; i < success_count; i++) {
        auto r = (DpuRequest*)dpuCons[wc[i].wr_id]->getMessage();
        r->wr_id = wc[i].wr_id;
        task_queue.push(r);
      }
      return success_count;
    }
  }

  void rpc_rsp_dpu(void* buffer, const DpuRequest& originReq) {

    auto rsp = (DpuResponse*)dpuCons[originReq.wr_id]->getSendPool();
    memcpy(rsp->buffer, buffer, 10);

    dpuCons[originReq.wr_id]->sendDpuResponse(rsp);                        
  }


private:
  DpuProxy(const DSMConfig &conf);
  ~DpuProxy();
  void init_dma_state();

  RemoteConnection *computeInfo;
  // ThreadConnection *hostCon[MAX_DPU_THREAD];
  DpuConnection *dpuCons[MAX_DPU_THREAD * 4];


  DmaConnectCtx dmaConCtx;
  static thread_local DmaConnect dmaCon;

  struct ibv_cq* completeQueues[MAX_DPU_THREAD];
  static thread_local ibv_cq* completeQueue;
  DpuCache *dpuCache;
  DSDpuKeeper *keeper; 
  RdmaContext ctx;
};
#endif /* __DPU_PROXY_H__ */
