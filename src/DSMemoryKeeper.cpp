#include "DSMemoryKeeper.h"

#include "Connection.h"
#include <iostream>

const char *DSMemoryKeeper::OK = "OK";
const char *DSMemoryKeeper::ServerPrefix = "SPre";


DSMemoryKeeper::DSMemoryKeeper(DirectoryConnection **dirCon, RemoteConnection *remoteCon,
            uint32_t maxCompute)
      : Keeper(maxCompute), dirCon(dirCon),
        remoteCon(remoteCon) {

    initLocalMeta();

    if (!connectMemcached()) {
      return;
    }
    enter();
    
    connectDpu();
    connect();

    initRouteRule();
}

void DSMemoryKeeper::initLocalMeta() {
  localMeta.dsmBase = (uint64_t)dirCon[0]->dsmPool;
  localMeta.lockBase = (uint64_t)dirCon[0]->lockPool;

  // per thread DIR
  std::cout << dirCon[0]->ctx.lid << "lid" << std::endl;
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    localMeta.dirTh[i].lid = dirCon[i]->ctx.lid;
    localMeta.dirTh[i].rKey = dirCon[i]->dsmMR->rkey;
    localMeta.dirTh[i].lock_rkey = dirCon[i]->lockMR->rkey;
    memcpy((char *)localMeta.dirTh[i].gid, (char *)(&dirCon[i]->ctx.gid),
           16 * sizeof(uint8_t));

    localMeta.dirUdQpn[i] = dirCon[i]->message->getQPN();
  }

}
void DSMemoryKeeper::enter() {
  memcached_return rc;
  uint64_t serverNum;

  while (true) {
    rc = memcached_increment(memc, SERVER_NUM_KEY, strlen(SERVER_NUM_KEY), 1,
                             &serverNum);
    if (rc == MEMCACHED_SUCCESS) {

      myNodeID = serverNum - 1;

      printf("I am server %d\n", myNodeID);
      return; 
    }
    fprintf(stderr, "Server %d Counld't incr value and get ID: %s, retry...\n",
            myNodeID, memcached_strerror(memc, rc));
    usleep(10000);
  }
}

void DSMemoryKeeper::connect() {
  size_t l;
  uint32_t flags;
  memcached_return rc;

  while (curServer < maxServer) {
    char *serverNumStr = memcached_get(memc, COMPUTE_NUM_KEY, strlen(COMPUTE_NUM_KEY),
                                       &l, &flags, &rc);
    if (rc != MEMCACHED_SUCCESS) {
      fprintf(stderr, "compute %d Counld't get ComputeNum: %s, retry\n", myNodeID,
              memcached_strerror(memc, rc));
      continue;
    }
    uint32_t computeNum = atoi(serverNumStr);
    free(serverNumStr);

    // /connect server K
    for (size_t k = curServer; k < computeNum; ++k) {
      connectNode(k);
      printf("I connect compute %zu\n", k);
    }
    curServer = computeNum;
  }
}

bool DSMemoryKeeper::connectNode(uint16_t remoteID) {

  setDataToRemote(remoteID);

  std::string setK = setKey(remoteID);
  memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));

  std::string getK = getKey(remoteID);
  ExchangeMeta *remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());

  setDataFromRemote(remoteID, remoteMeta);

  free(remoteMeta);
  return true;
}

void DSMemoryKeeper::connectDpu() {

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    auto &c = dirCon[i];

    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      localMeta.dirRcQpn2dpu[k] = c->data2dpu[k]->qp_num;
    }
  }
  std::string setK = "server-dpu" + std::to_string(getMyNodeID());
  memSet(setK.c_str(), setK.size(), (char *)(&localMeta), sizeof(localMeta));

  std::string getK = "dpu-server" + std::to_string(getMyNodeID());
  ExchangeMeta *remoteMeta = (ExchangeMeta *)memGet(getK.c_str(), getK.size());

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    auto &c = dirCon[i];

    for (int k = 0; k < MAX_DPU_THREAD; ++k) {
      auto &qp = c->data2dpu[k];
      assert(qp->qp_type == IBV_QPT_RC);
      modifyQPtoInit(qp, &c->ctx);
      modifyQPtoRTR(qp, remoteMeta->dpuRcQpn2dir[k],
                    remoteMeta->dpuTh[k].lid, remoteMeta->dpuTh[k].gid,
                    &c->ctx);
      modifyQPtoRTS(qp);
    }
  }
  free(remoteMeta);
  std::cout << "connect to dpu ok" << std::endl;
}

void DSMemoryKeeper::setDataToRemote(uint16_t remoteID) {
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    auto &c = dirCon[i];

    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      localMeta.dirRcQpn2app[i][k] = c->data2app[k][remoteID]->qp_num;
    }
  }

}

void DSMemoryKeeper::setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta) {
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    auto &c = dirCon[i];

    for (int k = 0; k < MAX_APP_THREAD; ++k) {
      auto &qp = c->data2app[k][remoteID];
      assert(qp->qp_type == IBV_QPT_RC);
      modifyQPtoInit(qp, &c->ctx);
      modifyQPtoRTR(qp, remoteMeta->appRcQpn2dir[k][i],
                    remoteMeta->appTh[k].lid, remoteMeta->appTh[k].gid,
                    &c->ctx);
      modifyQPtoRTS(qp);
    }
  }


  auto &info = remoteCon[remoteID];

  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    info.appRKey[i] = remoteMeta->appTh[i].rKey;
    info.appMessageQPN[i] = remoteMeta->appUdQpn[i];

    for (int k = 0; k < NR_DIRECTORY; ++k) {
      struct ibv_ah_attr ahAttr;
      fillAhAttr(&ahAttr, remoteMeta->appTh[i].lid, remoteMeta->appTh[i].gid,
                 &dirCon[k]->ctx);
      info.dirToAppAh[k][i] = ibv_create_ah(dirCon[k]->ctx.pd, &ahAttr);

      assert(info.dirToAppAh[k][i]);
    }
  }
}


void DSMemoryKeeper::initRouteRule() {
  std::string k =
      std::string(ServerPrefix) + std::to_string(this->getMyNodeID());
  memSet(k.c_str(), k.size(), getMyIP().c_str(), getMyIP().size());
}

void DSMemoryKeeper::barrier(const std::string &barrierKey) {

  std::string key = std::string("barrier-") + barrierKey;
  if (this->getMyNodeID() == 0) {
    memSet(key.c_str(), key.size(), "0", 1);
  }
  memFetchAndAdd(key.c_str(), key.size());
  while (true) {
    uint64_t v = std::stoull(memGet(key.c_str(), key.size()));
    if (v == this->getServerNR()) {
      return;
    }
  }
}

uint64_t DSMemoryKeeper::sum(const std::string &sum_key, uint64_t value) {
  std::string key_prefix = std::string("sum-") + sum_key;

  std::string key = key_prefix + std::to_string(this->getMyNodeID());
  memSet(key.c_str(), key.size(), (char *)&value, sizeof(value));

  uint64_t ret = 0;
  for (int i = 1; i < this->getServerNR(); ++i) {
    key = key_prefix + std::to_string(i);
    ret += *(uint64_t *)memGet(key.c_str(), key.size());
  }

  return ret;
}
