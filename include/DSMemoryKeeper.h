#ifndef __LINEAR_MEMORY_KEEPER__H__
#define __LINEAR_MEMORY_KEEPER__H__

#include <vector>

#include "Keeper.h"
#include "DSMKeeper.h"
struct ThreadConnection;
struct DirectoryConnection;
struct CacheAgentConnection;
struct RemoteConnection;

// struct ExPerThread {
//   uint16_t lid;
//   uint8_t gid[16];

//   uint32_t rKey;

//   uint32_t lock_rkey; //for directory on-chip memory 
// } __attribute__((packed));

// struct ExchangeMeta {
//   uint64_t dsmBase;
//   uint64_t cacheBase;
//   uint64_t lockBase;

//   ExPerThread appTh[MAX_APP_THREAD];
//   ExPerThread dirTh[NR_DIRECTORY];

//   uint32_t appUdQpn[MAX_APP_THREAD];
//   uint32_t dirUdQpn[NR_DIRECTORY];

//   uint32_t appRcQpn2dir[MAX_APP_THREAD][NR_DIRECTORY];

//   uint32_t dirRcQpn2app[NR_DIRECTORY][MAX_APP_THREAD];

//   // uint32_t app2dpu[MAX_APP_THREAD];
//   // uint32_t dpu2app[]

// }__attribute__((packed));

class DSMemoryKeeper : public Keeper {

private:
  static const char *OK;
  static const char *ServerPrefix;

  DirectoryConnection **dirCon;
  RemoteConnection *remoteCon;

  ExchangeMeta localMeta;

  std::vector<std::string> serverList;

  std::string setKey(uint16_t remoteID) {
    return "server" + std::to_string(getMyNodeID()) + "-" + std::to_string(remoteID);
  }

  std::string getKey(uint16_t remoteID) {
    return "compute" + std::to_string(remoteID) + "-" + std::to_string(getMyNodeID());
  }

  void initLocalMeta();

  void connectMySelf();
  void initRouteRule();

  void setDataToRemote(uint16_t remoteID);
  void setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta);

protected:
  virtual bool connectNode(uint16_t remoteID) override;
  void connectDpu();

public:
  DSMemoryKeeper(DirectoryConnection **dirCon, RemoteConnection *remoteCon,
            uint32_t maxCompute = 12);

  ~DSMemoryKeeper() { disconnectMemcached(); }
  void barrier(const std::string &barrierKey);
  uint64_t sum(const std::string &sum_key, uint64_t value);
};

#endif
