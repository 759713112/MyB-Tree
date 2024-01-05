#ifndef __DSM_MEMORY_H__
#define __DSM_MEMORY_H__

#include <atomic>

#include "Cache.h"
#include "Config.h"
#include "Connection.h"
#include "DSMemoryKeeper.h"
#include "GlobalAddress.h"
#include "LocalAllocator.h"
#include "RdmaBuffer.h"


class Directory;

class DSMemory {

public:
  // obtain netowrk resources for a thread
  void registerThread();

  // clear the network resources for all threads
  void resetThread() { appID.store(0); }

  static DSMemory *getInstance(const DSMConfig &conf);

  uint16_t getMyNodeID() { return myNodeID; }
  uint16_t getMyThreadID() { return thread_id; }
  uint16_t getClusterSize() { return conf.machineNR; }
  uint64_t getThreadTag() { return thread_tag; }

  
  uint64_t sum(uint64_t value) {
    static uint64_t count = 0;
    return keeper->sum(std::string("sum-") + std::to_string(count++), value);
  }

  // Memcached operations for sync
  size_t Put(uint64_t key, const void *value, size_t count) {

    std::string k = std::string("gam-") + std::to_string(key);
    keeper->memSet(k.c_str(), k.size(), (char *)value, count);
    return count;
  }

  size_t Get(uint64_t key, void *value) {

    std::string k = std::string("gam-") + std::to_string(key);
    size_t size;
    char *ret = keeper->memGet(k.c_str(), k.size(), &size);
    memcpy(value, ret, size);

    return size;
  }

private:
  DSMemory(const DSMConfig &conf);
  ~DSMemory();

  DSMConfig conf;
  std::atomic_int appID;
  Cache cache;

  static thread_local int thread_id;
  static thread_local char *rdma_buffer;
  static thread_local RdmaBuffer rbuf[define::kMaxCoro];
  static thread_local uint64_t thread_tag;

  uint64_t baseAddr;
  uint32_t myNodeID;

  RemoteConnection *remoteInfo;
  RemoteConnection *dpuConnectInfo;
  DirectoryConnection *dirCon[NR_DIRECTORY];
  DSMemoryKeeper *keeper;

  Directory *dirAgent[NR_DIRECTORY];

public:
  bool is_register() { return thread_id != -1; }
  void barrier(const std::string &ss) { keeper->barrier(ss); }

  char *get_rdma_buffer() { return rdma_buffer; }
  RdmaBuffer &get_rbuf(int coro_id) { return rbuf[coro_id]; }

};


#endif /* __DSM_H__ */
