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
  RemoteConnection *remoteCon;
  RemoteConnection *computeCon;
  ThreadConnection **thCon;
  
  ExchangeMeta localMeta;
  uint32_t maxCompute;

  struct program_core_objects *dma_state;

  std::vector<std::string> serverList;

  char *dma_export_desc;
  size_t dma_export_desc_len;

  void initLocalMeta();
  virtual void enter();
  virtual void connect();
  virtual bool connectNode(uint16_t remoteID) override { return false; };

  void initRouteRule();

  void setDataToRemote(uint16_t remoteID);
  void setDataFromRemote(uint16_t remoteID, ExchangeMeta *remoteMeta);
    
  void connectServer();
  void connectCompute();

  void init_dma_export(ExchangeMeta *remoteMeta);
public:
  DSDpuKeeper(ThreadConnection **thCon, RemoteConnection *remoteCon, RemoteConnection *computeCon, 
        uint32_t maxCompute = 12);

  ~DSDpuKeeper() { disconnectMemcached(); }
  void barrier(const std::string &barrierKey);
  uint64_t sum(const std::string &sum_key, uint64_t value);

  const char* get_dma_export_desc() { return this->dma_export_desc; }
  size_t get_dma_export_desc_len() { return this->dma_export_desc_len; }
};

#endif
