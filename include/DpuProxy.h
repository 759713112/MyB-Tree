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
class DSMKeeper;
class Directory;

class DpuProxy: public DSM {

public:
  // obtain netowrk resources for a thread
  // virtual void registerThread() override;
  // clear the network resources for all threads
  void resetThread() { appID.store(0); }

  static DpuProxy *getInstance(const DSMConfig &conf);
  virtual void rpc_call_dir(const RawMessage &m, uint16_t node_id,
                    uint16_t dir_id = 0) override;
private:
  DpuProxy(const DSMConfig &conf);
  ~DpuProxy();

  RemoteConnection *computeInfo;
  // ThreadConnection *hostCon[MAX_DPU_THREAD];
  // DpuConnection *dpuCon[MAX_DPU_THREAD];

  DSDpuKeeper *keeper; 

};
#endif /* __DPU_PROXY_H__ */
