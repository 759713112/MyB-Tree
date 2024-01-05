#include "DSMKeeper.h"

#include "Connection.h"
#include <iostream>
const char *DSMKeeper::OK = "OK";
const char *DSMKeeper::ServerPrefix = "SPre";

DSMKeeper::DSMKeeper(ThreadConnection **thCon, RemoteConnection *remoteCon,
            uint32_t maxServer, uint32_t maxCompute)
      : Keeper(maxServer), thCon(thCon),  maxCompute(maxCompute),
        remoteCon(remoteCon) {

    initLocalMeta();

    if (!connectMemcached()) {
      std::cout << "connect memcached error!" << std::endl;
      exit(-1);
      return;
    }
    enter();
    connect();

    initRouteRule();
}


void DSMKeeper::initLocalMeta() {
  localMeta.cacheBase = (uint64_t)thCon[0]->cachePool;

  // per thread APP
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    localMeta.appTh[i].lid = thCon[i]->ctx.lid;
    localMeta.appTh[i].rKey = thCon[i]->cacheMR->rkey;
    memcpy((char *)localMeta.appTh[i].gid, (char *)(&thCon[i]->ctx.gid),
           16 * sizeof(uint8_t));

    localMeta.appUdQpn[i] = thCon[i]->message->getQPN();
    localMeta.appUdQpn2dpu[i] = thCon[i]->dpuConnect->getQPN();
  }
}

void DSMKeeper::enter() {
  memcached_return rc;
  uint64_t computeNum;

  while (true) {
    rc = memcached_increment(memc, COMPUTE_NUM_KEY, strlen(COMPUTE_NUM_KEY), 1,
                             &computeNum);
    if (rc == MEMCACHED_SUCCESS) {

      myNodeID = computeNum - 1;

      printf("I am conmpute %d\n", myNodeID);
      return;
    }
    fprintf(stderr, "Compute %d Counld't incr value and get ID: %s, retry...\n",
            myNodeID, memcached_strerror(memc, rc));
    usleep(10000);
  }
}

void DSMKeeper::connect() {

  size_t l;
  uint32_t flags;
  memcached_return rc;

  while (curServer < maxServer) {
    char *serverNumStr = memcached_get(memc, SERVER_NUM_KEY,
                                       strlen(SERVER_NUM_KEY), &l, &flags, &rc);
    if (rc != MEMCACHED_SUCCESS) {
      fprintf(stderr, "compute %d Counld't get serverNum: %s, retry\n", myNodeID,
              memcached_strerror(memc, rc));
      continue;
    }
    uint32_t serverNum = atoi(serverNumStr);
    free(serverNumStr);

    // /connect server K
    for (size_t k = curServer; k < serverNum; ++k) {
      connectNode(k);
      printf("I connect server %zu\n", k);        
    }
    curServer = serverNum;
  }
}

bool DSMKeeper::connectNode(uint16_t remoteID) {

  setDataToRemote(remoteID);

  std::string setK = setKey(remoteID);
  memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));

  std::string getK = getKey(remoteID);
  ExchangeMeta *remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());

  setDataFromRemote(remoteID, remoteMeta);

  free(remoteMeta);

  //connect dpu
  setK = "compute-dpu:" + std::to_string(getMyNodeID()) + "-" + std::to_string(remoteID);
  memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));

  getK = "dpu-compute" + std::to_string(remoteID);
  remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());
  setDataFromRemoteDpu(remoteID, remoteMeta);

  return true;
}

void DSMKeeper::setDataToRemote(uint16_t remoteID) {
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    auto &c = thCon[i];
    for (int k = 0; k < NR_DIRECTORY; ++k) {
      localMeta.appRcQpn2dir[i][k] = c->data[k][remoteID]->qp_num;
    }
  
  }
}

void DSMKeeper::setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta) {
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    auto &c = thCon[i];
    for (int k = 0; k < NR_DIRECTORY; ++k) {
      auto &qp = c->data[k][remoteID];

      assert(qp->qp_type == IBV_QPT_RC);
      modifyQPtoInit(qp, &c->ctx);
      modifyQPtoRTR(qp, remoteMeta->dirRcQpn2app[k][i],
                    remoteMeta->dirTh[k].lid, remoteMeta->dirTh[k].gid,
                    &c->ctx);
      modifyQPtoRTS(qp);
    }
  }

  auto &info = remoteCon[remoteID];
  info.dsmBase = remoteMeta->dsmBase;
  info.cacheBase = remoteMeta->cacheBase;
  info.lockBase = remoteMeta->lockBase;

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    info.dsmRKey[i] = remoteMeta->dirTh[i].rKey;
    info.lockRKey[i] = remoteMeta->dirTh[i].lock_rkey;
    info.dirMessageQPN[i] = remoteMeta->dirUdQpn[i];

    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      struct ibv_ah_attr ahAttr;
      fillAhAttr(&ahAttr, remoteMeta->dirTh[i].lid, remoteMeta->dirTh[i].gid,
                 &thCon[k]->ctx);
      info.appToDirAh[k][i] = ibv_create_ah(thCon[k]->ctx.pd, &ahAttr);

      assert(info.appToDirAh[k][i]);
    }
  }
}

void DSMKeeper::setDataFromRemoteDpu(uint16_t remoteID, ExchangeMeta *remoteMeta) {
  auto &info = remoteCon[remoteID];
  for (int i = 0; i < MAX_DPU_THREAD; ++i) {
    info.dpuRequestQPN[i] = remoteMeta->dpuUdQpn2app[i];
    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      struct ibv_ah_attr ahAttr;
      std::cout << remoteMeta->dpuTh[i].lid << "lid" << std::endl;
      fillAhAttr(&ahAttr, remoteMeta->dpuTh[i].lid, remoteMeta->dpuTh[i].gid,
                &thCon[k]->ctx);
      info.appToDpuAh[k][i] = ibv_create_ah(thCon[k]->ctx.pd, &ahAttr);

      assert(info.appToDpuAh[k][i]);
    }
  }
  std::cout << "connect dpu ok" << std::endl;

}

void DSMKeeper::initRouteRule() {

  std::string k =
      std::string(ServerPrefix) + std::to_string(this->getMyNodeID());
  memSet(k.c_str(), k.size(), getMyIP().c_str(), getMyIP().size());
}

void DSMKeeper::barrier(const std::string &barrierKey) {

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

uint64_t DSMKeeper::sum(const std::string &sum_key, uint64_t value) {
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
