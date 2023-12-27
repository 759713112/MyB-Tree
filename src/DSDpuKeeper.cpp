#include "DSDpuKeeper.h"

#include "Connection.h"
#include <iostream>
const char *DSDpuKeeper::OK = "OK";
const char *DSDpuKeeper::ServerPrefix = "SPre";


DSDpuKeeper::DSDpuKeeper(ThreadConnection **thCon, RemoteConnection *remoteCon, RemoteConnection *computeCon, 
        uint32_t maxCompute)
      : Keeper(maxCompute), thCon(thCon), remoteCon(remoteCon),         computeCon(computeCon), maxCompute(maxCompute) {
    initLocalMeta();

    if (!connectMemcached()) {
      return;
    }
    enter();
    connect();
    // initRouteRule();
}

void DSDpuKeeper::initLocalMeta() {
  localMeta.cacheBase = (uint64_t)thCon[0]->cachePool;


  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    localMeta.dpuTh[i].lid = thCon[i]->ctx.lid;

    localMeta.dpuTh[i].rKey = thCon[i]->cacheMR->rkey;
    memcpy((char *)localMeta.dpuTh[i].gid, (char *)(&thCon[i]->ctx.gid),
           16 * sizeof(uint8_t));

    localMeta.appUdQpn[i] = thCon[i]->message->getQPN();
  }
  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    localMeta.dpuRcQpn2dir[i] = thCon[i]->data[0][0]->qp_num;
    localMeta.dpuUdQpn2app[i] = thCon[i]->dpuConnect->getQPN();
  }
}

void DSDpuKeeper::enter() {
  memcached_return rc;
  uint64_t dpuNum;

  while (true) {
    rc = memcached_increment(memc, DPU_NUM_KEY, strlen(DPU_NUM_KEY), 1,
                             &dpuNum);
    if (rc == MEMCACHED_SUCCESS) {

      myNodeID = dpuNum - 1;

      printf("I am dpu %d\n", myNodeID);
      return;
    }
    fprintf(stderr, "Dpu %d Counld't incr value and get ID: %s, retry...\n",
            myNodeID, memcached_strerror(memc, rc));
    usleep(10000);
  }
}

void DSDpuKeeper::connect() {
  connectServer();
  connectCompute();
}

void DSDpuKeeper::setDataToRemote(uint16_t remoteID) {
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    auto &c = thCon[i];
    for (int k = 0; k < NR_DIRECTORY; ++k) {
      localMeta.appRcQpn2dir[i][k] = c->data[k][remoteID]->qp_num;
    }
  
  }
}

void DSDpuKeeper::connectServer() {
    std::string setK = "dpu-server" + std::to_string(getMyNodeID());
    memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));

    std::string getK = "server-dpu" + std::to_string(getMyNodeID());
    ExchangeMeta *remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());

    setDataFromRemote(getMyNodeID(), remoteMeta);
    free(remoteMeta);
    std::cout << "connect to server ok" << std::endl;
}


void DSDpuKeeper::connectCompute()
{
    std::string setK = "dpu-compute" + std::to_string(getMyNodeID());
    memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));
    //connect to all compute node
    for (size_t k = 0; k < maxCompute; k++) {
      std::string getK = "compute-dpu:" + std::to_string(k) + "-" + std::to_string(getMyNodeID());
      ExchangeMeta *remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());
      auto &info = computeCon[k];
      for (int i = 0; i < MAX_APP_THREAD; ++i) {
        info.appRKey[i] = remoteMeta->appTh[i].rKey;
        info.appRequestQPN[i] = remoteMeta->appUdQpn2dpu[i];

        for (int j = 0; j < MAX_DPU_THREAD; ++j) {
          struct ibv_ah_attr ahAttr;
          fillAhAttr(&ahAttr, remoteMeta->appTh[i].lid, remoteMeta->appTh[i].gid,
                    &thCon[j]->ctx);
          info.dpuToAppAh[j][i] = ibv_create_ah(thCon[j]->ctx.pd, &ahAttr);

          assert(info.dpuToAppAh[j][i]);
        }
      }
      std::cout << "connect compute " << k << " ok" << std::endl;
    }
}

void DSDpuKeeper::setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta) {
  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    auto &c = thCon[i];
    auto &qp = c->data[0][0];

    assert(qp->qp_type == IBV_QPT_RC);
    modifyQPtoInit(qp, &c->ctx);
    modifyQPtoRTR(qp, remoteMeta->dirRcQpn2dpu[i],
                  remoteMeta->dirTh[0].lid, remoteMeta->dirTh[0].gid,
                  &c->ctx);
    modifyQPtoRTS(qp);
  }

  auto &info = remoteCon[remoteID];
  info.dsmBase = remoteMeta->dsmBase;
  info.cacheBase = remoteMeta->cacheBase;
  info.lockBase = remoteMeta->lockBase;

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    info.dsmRKey[i] = remoteMeta->dirTh[i].rKey;
    info.lockRKey[i] = remoteMeta->dirTh[i].lock_rkey;
    info.dirMessageQPN[i] = remoteMeta->dirUdQpn[i];

    RdmaContext ctx;
    
    createContext(&ctx);
    RdmaContext ctx2;
    
    createContext(&ctx2);
    uint8_t gid[16];
        memcpy((char *) gid, (char *)(&ctx.gid),
           16 * sizeof(uint8_t));
    struct ibv_ah_attr ahAttr;
    fillAhAttr(&ahAttr, ctx.lid, gid, &ctx);
    auto x = ibv_create_ah(ctx.pd, &ahAttr);

    std::cout << &ctx2 << std::endl;
    if (x == nullptr) {
        std::cout << "error" << std::endl;
        exit(-1);
    }
    for (int k = 0; k < MAX_DPU_THREAD; ++k) {
      auto &c = thCon[k];

      //奇怪的bug 先这样
      c->ctx.port = 1;
      c->ctx.gidIndex = 3;

      struct ibv_ah_attr ahAttr;
      fillAhAttr(&ahAttr, remoteMeta->dirTh[i].lid, remoteMeta->dirTh[i].gid,
                 &c->ctx);
      info.appToDirAh[k][i] = ibv_create_ah(c->ctx.pd, &ahAttr);
      assert(info.appToDirAh[k][i]);
    }

  }
}


void DSDpuKeeper::barrier(const std::string &barrierKey) {

  std::string key = std::string("barrier-") + barrierKey;
  if (this->getMyNodeID() == 0) {
    memSet(key.c_str(), key.size(), "0", 1);
  }
  memFetchAndAdd(key.c_str(), key.size());
  while (true) {
    uint64_t v = std::stoull(memGet(key.c_str(), key.size()));
    if (v == this->maxCompute) {
      return;
    }
  }
}

uint64_t DSDpuKeeper::sum(const std::string &sum_key, uint64_t value) {
  std::string key_prefix = std::string("sum-") + sum_key;

  std::string key = key_prefix + std::to_string(this->getMyNodeID());
  memSet(key.c_str(), key.size(), (char *)&value, sizeof(value));

  uint64_t ret = 0;
  for (int i = 1; i < this->maxCompute; ++i) {
    key = key_prefix + std::to_string(i);
    ret += *(uint64_t *)memGet(key.c_str(), key.size());
  }

  return ret;
}
