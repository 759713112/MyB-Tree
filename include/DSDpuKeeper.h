#ifndef __DS_DPU_KEEPER__H__
#define __DS_DPU_KEEPER__H__

#include <vector>

#include "Keeper.h"
#include "DSMKeeper.h"
#include "DpuConnection.h"

struct ThreadConnection;
struct DirectoryConnection;
struct CacheAgentConnection;
struct RemoteConnection;


class DSDpuKeeper : public Keeper {

private:
  static const char *OK;
  static const char *ServerPrefix;

  ThreadConnection **thCon;
  DpuConnection **dpuCon;
  RemoteConnection *remoteCon;
  RemoteConnection *computeCon;

  ExchangeMeta localMeta;
  uint32_t maxCompute;

  std::vector<std::string> serverList;



  void initLocalMeta();

  void initRouteRule();

  void setDataToRemote(uint16_t remoteID);
  void setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta);

protected:
  virtual bool connectNode(uint16_t remoteID) override { return false; }
  void connectServer();
  void connectCompute();

public:
  DSDpuKeeper(ThreadConnection **thCon, DpuConnection **dpuCon, RemoteConnection *remoteCon, RemoteConnection *computeCon, 
        uint32_t maxCompute = 12);


  ~DSDpuKeeper() { disconnectMemcached(); }
  void barrier(const std::string &barrierKey);
  uint64_t sum(const std::string &sum_key, uint64_t value);
};

#endif
