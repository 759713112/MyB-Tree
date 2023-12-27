#ifndef __KEEPER__H__
#define __KEEPER__H__

#include <assert.h>
#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <thread>

#include <libmemcached/memcached.h>

#include "Config.h"
#include "Debug.h"
#include "Rdma.h"

struct ExPerThread {
  uint16_t lid;
  uint8_t gid[16];

  uint32_t rKey;

  uint32_t lock_rkey; //for directory on-chip memory 
} __attribute__((packed));

struct ExchangeMeta {
  uint64_t dsmBase;
  uint64_t cacheBase;
  uint64_t lockBase;

  ExPerThread appTh[MAX_APP_THREAD];
  ExPerThread dirTh[NR_DIRECTORY];

  uint32_t appUdQpn[MAX_APP_THREAD];
  uint32_t dirUdQpn[NR_DIRECTORY];

  uint32_t appRcQpn2dir[MAX_APP_THREAD][NR_DIRECTORY];

  uint32_t dirRcQpn2app[NR_DIRECTORY][MAX_APP_THREAD];


  ExPerThread dpuTh[MAX_DPU_THREAD];
  uint32_t dpuRcQpn2dir[MAX_DPU_THREAD];
  uint32_t dpuUdQpn2app[MAX_DPU_THREAD];

  uint32_t dirRcQpn2dpu[MAX_DPU_THREAD];

  uint32_t appUdQpn2dpu[MAX_APP_THREAD];
  // uint32_t app2dpu[MAX_DPU_THREAD];
  // uint32_t dpu2app[]

}__attribute__((packed));

class Keeper {

protected:
  bool connectMemcached();
  bool disconnectMemcached();
  void initRouteRule();

  virtual void initLocalMeta() = 0;
  virtual void connect() = 0;
  virtual void enter() = 0;
  virtual bool connectNode(uint16_t remoteID) = 0;

  memcached_st *memc;
  uint32_t maxServer;
  uint16_t curServer;
  uint16_t myNodeID;
  std::string myIP;
  uint16_t myPort;
public:
  static const char *SERVER_NUM_KEY;
  static const char *COMPUTE_NUM_KEY;
  static const char *DPU_NUM_KEY;
  static const char *MY_NUM_KEY;
  Keeper(uint32_t maxServer = 12);
  void init();
  ~Keeper();

  uint16_t getMyNodeID() const { return this->myNodeID; }
  uint16_t getServerNR() const { return this->maxServer; }
  uint16_t getMyPort() const { return this->myPort; }

  std::string getMyIP() const { return this->myIP; }


  void memSet(const char *key, uint32_t klen, const char *val, uint32_t vlen);
  
  char *memGet(const char *key, uint32_t klen, size_t *v_size = nullptr);
  uint64_t memFetchAndAdd(const char *key, uint32_t klen);
};

#endif
