#ifndef __LINEAR_KEEPER__H__
#define __LINEAR_KEEPER__H__

#include <vector>

#include "Keeper.h"
struct ThreadConnection;
struct DirectoryConnection;
struct CacheAgentConnection;
struct RemoteConnection;

class DSMKeeper : public Keeper {

private:
  static const char *OK;
  static const char *ServerPrefix;

  ThreadConnection **thCon;


  ExchangeMeta localMeta;
  uint32_t maxCompute;
  RemoteConnection *remoteCon;

  std::vector<std::string> serverList;

  std::string setKey(uint16_t remoteID) {
    return "compute" + std::to_string(getMyNodeID()) + "-" + std::to_string(remoteID);
  }

  std::string getKey(uint16_t remoteID) {
    return "server" + std::to_string(remoteID) + "-" + std::to_string(getMyNodeID());
  }

  void initRouteRule();

  void setDataToRemote(uint16_t remoteID);
  void setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta);
  void setDataFromRemoteDpu(uint16_t remoteID, ExchangeMeta *remoteMeta);

protected:
  void initLocalMeta();
  virtual void enter() override;
  virtual void connect() override;
  virtual bool connectNode(uint16_t remoteID) override;

public:
  DSMKeeper(ThreadConnection **thCon, RemoteConnection *remoteCon,
            uint32_t maxServer = 12, uint32_t maxCompute = 12);


  ~DSMKeeper() { disconnectMemcached(); }
  void barrier(const std::string &barrierKey);
  uint64_t sum(const std::string &sum_key, uint64_t value);
};

#endif
