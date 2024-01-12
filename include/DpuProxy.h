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
  
  void readByDma(char* buffer, uint64_t offset, size_t size, bool signal,
              CoroContext *ctx);
  void simple_proxy() {
    
  }

private:
  DpuProxy(const DSMConfig &conf);
  ~DpuProxy();
  void init_dma_state();

  RemoteConnection *computeInfo;
  // ThreadConnection *hostCon[MAX_DPU_THREAD];
  // DpuConnection *dpuCon[MAX_DPU_THREAD];

  DmaConnectCtx dmaCon;

  DSDpuKeeper *keeper; 
};
#endif /* __DPU_PROXY_H__ */
