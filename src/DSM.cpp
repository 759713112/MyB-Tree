
#include "DSM.h"
#include "Directory.h"
#include "HugePageAlloc.h"

#include "DSMKeeper.h"

#include <algorithm>

thread_local int DSM::thread_id = -1;
thread_local ThreadConnection *DSM::iCon = nullptr;
thread_local char *DSM::rdma_buffer = nullptr;
thread_local LocalAllocator DSM::local_allocator;
thread_local RdmaBuffer DSM::rbuf[define::kMaxCoro];
thread_local uint64_t DSM::thread_tag = 0;

DSM *DSM::getInstance(const DSMConfig &conf) {
  static DSM *dsm = nullptr;
  static WRLock lock;

  lock.wLock();
  if (!dsm) {
    dsm = new DSM(conf);
    dsm->init();
  } else {
  }
  lock.wUnlock();

  return dsm;
}

DSM::DSM(const DSMConfig &conf)
    : conf(conf), appID(0), cache(conf.cacheConfig) { }

void DSM::init() {
  remoteInfo = new RemoteConnection[conf.memoryNR];
  
  Debug::notifyInfo("number of computeNode: %d, number of memoryNode, %d", conf.machineNR, conf.memoryNR);

  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    thCon[i] =
        new ThreadConnection(i, (void *)cache.data, cache.size * define::GB,
                            conf.memoryNR, remoteInfo);
  }
  keeper = new DSMKeeper(thCon, remoteInfo, conf.memoryNR, conf.machineNR);

  myNodeID = keeper->getMyNodeID();

  auto t =  new std::thread(&DSM::catch_root_change);
  init_socket();
}

DSM::~DSM() {}

void DSM::registerThread() {

  static bool has_init[MAX_APP_THREAD];

  if (thread_id != -1)
    return;

  thread_id = appID.fetch_add(1);
  thread_tag = thread_id + (((uint64_t)this->getMyNodeID()) << 32) + 1;

  iCon = thCon[thread_id];

  if (!has_init[thread_id]) {
    has_init[thread_id] = true;
  }

  rdma_buffer = (char *)cache.data + thread_id * 12 * define::MB;

  for (int i = 0; i < define::kMaxCoro; ++i) {
    rbuf[i].set_buffer(rdma_buffer + i * define::kPerCoroRdmaBuf);
  }
}


void DSM::read(char *buffer, GlobalAddress gaddr, size_t size, bool signal,
               CoroContext *ctx) {
  if (ctx == nullptr) {
    rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
             remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
             iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], signal);
  } else {
    rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
             remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
             iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], true,
             ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::read_sync(char *buffer, GlobalAddress gaddr, size_t size,
                    CoroContext *ctx) {
  read(buffer, gaddr, size, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::write(const char *buffer, GlobalAddress gaddr, size_t size,
                bool signal, CoroContext *ctx) {

  if (ctx == nullptr) {
    rdmaWrite(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
              remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
              iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], -1, signal);
  } else {
    rdmaWrite(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
              remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
              iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], -1, true,
              ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::write_sync(const char *buffer, GlobalAddress gaddr, size_t size,
                     CoroContext *ctx) {
  write(buffer, gaddr, size, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::fill_keys_dest(RdmaOpRegion &ror, GlobalAddress gaddr, bool is_chip) {
  ror.lkey = iCon->cacheLKey;
  if (is_chip) {
    ror.dest = remoteInfo[gaddr.nodeID].lockBase + gaddr.offset;
    ror.remoteRKey = remoteInfo[gaddr.nodeID].lockRKey[0];
  } else {
    ror.dest = remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset;
    ror.remoteRKey = remoteInfo[gaddr.nodeID].dsmRKey[0];
  }
}

void DSM::write_batch(RdmaOpRegion *rs, int k, bool signal, CoroContext *ctx) {

  int node_id = -1;
  for (int i = 0; i < k; ++i) {

    GlobalAddress gaddr;
    gaddr.val = rs[i].dest;
    node_id = gaddr.nodeID;
    fill_keys_dest(rs[i], gaddr, rs[i].is_on_chip);
  }

  if (ctx == nullptr) {
    rdmaWriteBatch(iCon->data[0][node_id], rs, k, signal);
  } else {
    rdmaWriteBatch(iCon->data[0][node_id], rs, k, true, ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::write_batch_sync(RdmaOpRegion *rs, int k, CoroContext *ctx) {
  write_batch(rs, k, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::write_faa(RdmaOpRegion &write_ror, RdmaOpRegion &faa_ror,
                    uint64_t add_val, bool signal, CoroContext *ctx) {
  int node_id;
  {
    GlobalAddress gaddr;
    gaddr.val = write_ror.dest;
    node_id = gaddr.nodeID;

    fill_keys_dest(write_ror, gaddr, write_ror.is_on_chip);
  }
  {
    GlobalAddress gaddr;
    gaddr.val = faa_ror.dest;

    fill_keys_dest(faa_ror, gaddr, faa_ror.is_on_chip);
  }
  if (ctx == nullptr) {
    rdmaWriteFaa(iCon->data[0][node_id], write_ror, faa_ror, add_val, signal);
  } else {
    rdmaWriteFaa(iCon->data[0][node_id], write_ror, faa_ror, add_val, true,
                 ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}
void DSM::write_faa_sync(RdmaOpRegion &write_ror, RdmaOpRegion &faa_ror,
                         uint64_t add_val, CoroContext *ctx) {
  write_faa(write_ror, faa_ror, add_val, true, ctx);
  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::write_cas(RdmaOpRegion &write_ror, RdmaOpRegion &cas_ror,
                    uint64_t equal, uint64_t val, bool signal,
                    CoroContext *ctx) {
  int node_id;
  {
    GlobalAddress gaddr;
    gaddr.val = write_ror.dest;
    node_id = gaddr.nodeID;

    fill_keys_dest(write_ror, gaddr, write_ror.is_on_chip);
  }
  {
    GlobalAddress gaddr;
    gaddr.val = cas_ror.dest;

    fill_keys_dest(cas_ror, gaddr, cas_ror.is_on_chip);
  }
  if (ctx == nullptr) {
    rdmaWriteCas(iCon->data[0][node_id], write_ror, cas_ror, equal, val,
                 signal);
  } else {
    rdmaWriteCas(iCon->data[0][node_id], write_ror, cas_ror, equal, val, true,
                 ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}
void DSM::write_cas_sync(RdmaOpRegion &write_ror, RdmaOpRegion &cas_ror,
                         uint64_t equal, uint64_t val, CoroContext *ctx) {
  write_cas(write_ror, cas_ror, equal, val, true, ctx);
  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::cas_read(RdmaOpRegion &cas_ror, RdmaOpRegion &read_ror,
                   uint64_t equal, uint64_t val, bool signal,
                   CoroContext *ctx) {

  int node_id;
  {
    GlobalAddress gaddr;
    gaddr.val = cas_ror.dest;
    node_id = gaddr.nodeID;
    fill_keys_dest(cas_ror, gaddr, cas_ror.is_on_chip);
  }
  {
    GlobalAddress gaddr;
    gaddr.val = read_ror.dest;
    fill_keys_dest(read_ror, gaddr, read_ror.is_on_chip);
  }

  if (ctx == nullptr) {
    rdmaCasRead(iCon->data[0][node_id], cas_ror, read_ror, equal, val, signal);
  } else {
    rdmaCasRead(iCon->data[0][node_id], cas_ror, read_ror, equal, val, true,
                ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

bool DSM::cas_read_sync(RdmaOpRegion &cas_ror, RdmaOpRegion &read_ror,
                        uint64_t equal, uint64_t val, CoroContext *ctx) {
  cas_read(cas_ror, read_ror, equal, val, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }

  return equal == *(uint64_t *)cas_ror.source;
}

void DSM::cas(GlobalAddress gaddr, uint64_t equal, uint64_t val,
              uint64_t *rdma_buffer, bool signal, CoroContext *ctx) {

  if (ctx == nullptr) {
    rdmaCompareAndSwap(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                       remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, equal,
                       val, iCon->cacheLKey,
                       remoteInfo[gaddr.nodeID].dsmRKey[0], signal);
  } else {
    rdmaCompareAndSwap(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                       remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, equal,
                       val, iCon->cacheLKey,
                       remoteInfo[gaddr.nodeID].dsmRKey[0], true, ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

bool DSM::cas_sync(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                   uint64_t *rdma_buffer, CoroContext *ctx) {
  cas(gaddr, equal, val, rdma_buffer, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }

  return equal == *rdma_buffer;
}

void DSM::cas_mask(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                   uint64_t *rdma_buffer, uint64_t mask, bool signal) {
  rdmaCompareAndSwapMask(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                         remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, equal,
                         val, iCon->cacheLKey,
                         remoteInfo[gaddr.nodeID].dsmRKey[0], mask, signal);
}

bool DSM::cas_mask_sync(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                        uint64_t *rdma_buffer, uint64_t mask) {
  cas_mask(gaddr, equal, val, rdma_buffer, mask);
  ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);

  return (equal & mask) == (*rdma_buffer & mask);
}

void DSM::faa_boundary(GlobalAddress gaddr, uint64_t add_val,
                       uint64_t *rdma_buffer, uint64_t mask, bool signal,
                       CoroContext *ctx) {
  if (ctx == nullptr) {
    rdmaFetchAndAddBoundary(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                            remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset,
                            add_val, iCon->cacheLKey,
                            remoteInfo[gaddr.nodeID].dsmRKey[0], mask, signal);
  } else {
    rdmaFetchAndAddBoundary(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                            remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset,
                            add_val, iCon->cacheLKey,
                            remoteInfo[gaddr.nodeID].dsmRKey[0], mask, true,
                            ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::faa_boundary_sync(GlobalAddress gaddr, uint64_t add_val,
                            uint64_t *rdma_buffer, uint64_t mask,
                            CoroContext *ctx) {
  faa_boundary(gaddr, add_val, rdma_buffer, mask, true, ctx);
  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::read_dm(char *buffer, GlobalAddress gaddr, size_t size, bool signal,
                  CoroContext *ctx) {

  if (ctx == nullptr) {
    rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
             remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, size,
             iCon->cacheLKey, remoteInfo[gaddr.nodeID].lockRKey[0], signal);
  } else {
    rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
             remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, size,
             iCon->cacheLKey, remoteInfo[gaddr.nodeID].lockRKey[0], true,
             ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::read_dm_sync(char *buffer, GlobalAddress gaddr, size_t size,
                       CoroContext *ctx) {
  read_dm(buffer, gaddr, size, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::write_dm(const char *buffer, GlobalAddress gaddr, size_t size,
                   bool signal, CoroContext *ctx) {
  if (ctx == nullptr) {
    rdmaWrite(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
              remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, size,
              iCon->cacheLKey, remoteInfo[gaddr.nodeID].lockRKey[0], -1,
              signal);
  } else {
    rdmaWrite(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
              remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, size,
              iCon->cacheLKey, remoteInfo[gaddr.nodeID].lockRKey[0], -1, true,
              ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::write_dm_sync(const char *buffer, GlobalAddress gaddr, size_t size,
                        CoroContext *ctx) {
  write_dm(buffer, gaddr, size, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

void DSM::cas_dm(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                 uint64_t *rdma_buffer, bool signal, CoroContext *ctx) {

  if (ctx == nullptr) {
    rdmaCompareAndSwap(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                       remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, equal,
                       val, iCon->cacheLKey,
                       remoteInfo[gaddr.nodeID].lockRKey[0], signal);
  } else {
    rdmaCompareAndSwap(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                       remoteInfo[gaddr.nodeID].lockBase + gaddr.offset, equal,
                       val, iCon->cacheLKey,
                       remoteInfo[gaddr.nodeID].lockRKey[0], true,
                       ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

bool DSM::cas_dm_sync(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                      uint64_t *rdma_buffer, CoroContext *ctx) {
  cas_dm(gaddr, equal, val, rdma_buffer, true, ctx);

  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }

  return equal == *rdma_buffer;
}

void DSM::cas_dm_mask(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                      uint64_t *rdma_buffer, uint64_t mask, bool signal) {
  rdmaCompareAndSwapMask(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                         remoteInfo[gaddr.nodeID].lockBase + gaddr.offset,
                         equal, val, iCon->cacheLKey,
                         remoteInfo[gaddr.nodeID].lockRKey[0], mask, signal);
}

bool DSM::cas_dm_mask_sync(GlobalAddress gaddr, uint64_t equal, uint64_t val,
                           uint64_t *rdma_buffer, uint64_t mask) {
  cas_dm_mask(gaddr, equal, val, rdma_buffer, mask);
  ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);

  return (equal & mask) == (*rdma_buffer & mask);
}

void DSM::faa_dm_boundary(GlobalAddress gaddr, uint64_t add_val,
                          uint64_t *rdma_buffer, uint64_t mask, bool signal,
                          CoroContext *ctx) {
  if (ctx == nullptr) {

    rdmaFetchAndAddBoundary(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                            remoteInfo[gaddr.nodeID].lockBase + gaddr.offset,
                            add_val, iCon->cacheLKey,
                            remoteInfo[gaddr.nodeID].lockRKey[0], mask, signal);
  } else {
    rdmaFetchAndAddBoundary(iCon->data[0][gaddr.nodeID], (uint64_t)rdma_buffer,
                            remoteInfo[gaddr.nodeID].lockBase + gaddr.offset,
                            add_val, iCon->cacheLKey,
                            remoteInfo[gaddr.nodeID].lockRKey[0], mask, true,
                            ctx->coro_id);
    (*ctx->yield)(*ctx->master);
  }
}

void DSM::faa_dm_boundary_sync(GlobalAddress gaddr, uint64_t add_val,
                               uint64_t *rdma_buffer, uint64_t mask,
                               CoroContext *ctx) {
  faa_dm_boundary(gaddr, add_val, rdma_buffer, mask, true, ctx);
  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
}

uint64_t DSM::poll_rdma_cq(int count) {
  ibv_wc wc;
  pollWithCQ(iCon->cq, count, &wc);

  return wc.wr_id;
}

bool DSM::poll_rdma_cq_once(uint64_t &wr_id) {
  ibv_wc wc;
  int res = pollOnce(iCon->cq, 1, &wc);

  wr_id = wc.wr_id;

  return res == 1;
}
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
extern GlobalAddress g_root_ptr;
extern int g_root_level;
extern bool enable_cache;
void DSM::catch_root_change() {
  std::cout << "start catch broadcast" << std::endl;
  int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Error creating socket\n";
        exit(-1);
    }

    int broadcastEnable = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1) {
        std::cerr << "Error setting socket options\n";
        close(udpSocket);
        exit(-1);
    }

    struct sockaddr_in serverAddress, clientAddress;
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345); // Port number
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    int val = 1;
    setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
    if (bind(udpSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket\n";
        close(udpSocket);
        exit(-1);
    }

    char buffer[1024];
    while (true) {
      
      socklen_t clientAddressLength = sizeof(clientAddress);
      ssize_t receivedBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddress, &clientAddressLength);
      if (receivedBytes == -1 || receivedBytes != sizeof(RawMessage)) {
          std::cerr << "Error receiving message\n";
          close(udpSocket);
          exit(-1);

      }
      RawMessage* m = (RawMessage*)buffer;
      if (g_root_level < m->level) {
          g_root_ptr = m->addr;
          g_root_level = m->level;
          if (g_root_level >= 3) {
            enable_cache = true;
          }
          std::cout << "Received change root to" << g_root_ptr << std::endl;
      }
    } 
}


void DSM::init_socket() {
  udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Error creating socket\n";
        exit(-1);
    }

    int broadcastEnable = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1) {
        std::cerr << "Error setting socket options\n";
        close(udpSocket);
        exit(-1);
    }
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345); // Port number
    serverAddress.sin_addr.s_addr = INADDR_BROADCAST; // Broadcast address
}
void DSM::broadcast_new_root_to_client(const RawMessage &m) {
    if (sendto(udpSocket, &m, sizeof(m), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error sending message\n";
        close(udpSocket);
        exit(-1);
    }
    std::cout << "brocast new root" << std::endl;
}

DpuRequest* DSM::get_sendpool_to_dpu(size_t size) {
  auto r = (DpuRequest*)(iCon->dpuConnect->getSendPool());
  r->app_id = thread_id;
  r->node_id = getMyNodeID();
  return r;
}

bool DSM::send_request_to_dpu(DpuRequest* r, size_t size) {
  iCon->dpuConnect->sendDpuRequest(
      r, remoteInfo[r->node_id].dpuRequestQPN[r->node_id % MAX_DPU_THREAD], 
      remoteInfo[r->node_id].appToDpuAh[r->app_id][r->node_id]
  );
}

