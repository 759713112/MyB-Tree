
#include "DSMemory.h"
#include "Directory.h"
#include "HugePageAlloc.h"

#include <algorithm>


DSMemory *DSMemory::getInstance(const DSMConfig &conf) {
  static DSMemory dsm(conf);
  return &dsm;
  // static DSM *dsm = nullptr;
  // static WRLock lock;

  // lock.wLock();
  // if (!dsm) {
  //   dsm = new DSM(conf);
  // } else {
  // }
  // lock.wUnlock();

  // return dsm;
}

DSMemory::DSMemory(const DSMConfig &conf)
    : conf(conf), appID(0), cache(conf.cacheConfig) {


  remoteInfo = new RemoteConnection[conf.machineNR];
  baseAddr = (uint64_t)hugePageAlloc(conf.dsmSize * define::GB);
  Debug::notifyInfo("shared memory size: %dGB, 0x%lx", conf.dsmSize, baseAddr);
  Debug::notifyInfo("cache size: %dGB", conf.cacheConfig.cacheSize);

  // warmup
  // memset((char *)baseAddr, 0, conf.dsmSize * define::GB);
  for (uint64_t i = baseAddr; i < baseAddr + conf.dsmSize * define::GB;
      i += 2 * define::MB) {
    *(char *)i = 0;
  }

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    dirCon[i] =
        new DirectoryConnection(i, (void *)baseAddr, conf.dsmSize * define::GB,
                                conf.machineNR, remoteInfo, true);
  }
  // clear up first chunk
  memset((char *)baseAddr, 0, define::kChunkSize);


  //initRDMAConnection();
  
  keeper = new DSMemoryKeeper(dirCon, remoteInfo, conf.machineNR);

  myNodeID = keeper->getMyNodeID();

  Debug::notifyInfo("number of threads on memory node: %d", NR_DIRECTORY);
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    dirAgent[i] =
        new Directory(dirCon[i], remoteInfo, conf.machineNR, i, myNodeID);
  }

  keeper->barrier("DSM-init");
}

DSMemory::~DSMemory() {}


// void DSMemory::initRDMAConnection() {

//   Debug::notifyInfo("number of servers (colocated MN/CN): %d", conf.machineNR);

//   remoteInfo = new RemoteConnection[conf.machineNR];

//   for (int i = 0; i < MAX_APP_THREAD; ++i) {
//     thCon[i] =
//         new ThreadConnection(i, (void *)cache.data, cache.size * define::GB,
//                              conf.machineNR, remoteInfo);
//   }

//   if (conf.isMemoryNode) {
//     for (int i = 0; i < NR_DIRECTORY; ++i) {
//     dirCon[i] =
//         new DirectoryConnection(i, (void *)baseAddr, conf.dsmSize * define::GB,
//                                 conf.machineNR, remoteInfo, true);
//     }
//   }
  

//   keeper = new DSMKeeper(thCon, dirCon, remoteInfo, conf.machineNR);

//   myNodeID = keeper->getMyNodeID();
// }

