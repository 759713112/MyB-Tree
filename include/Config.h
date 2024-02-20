#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "Common.h"

class CacheConfig {
public:
  uint32_t cacheSize;

  CacheConfig(uint32_t cacheSize = 1) : cacheSize(cacheSize) {}
};

class DSMConfig {
public:
  CacheConfig cacheConfig;
  uint32_t machineNR; //计算节点个数
  uint32_t memoryNR; //内存节点个数
  uint64_t dsmSize; // G
  bool isMemoryNode;

  DSMConfig(const CacheConfig &cacheConfig = CacheConfig(),
            uint32_t machineNR = 2, uint32_t memoryNR = 2, uint64_t dsmSize = 32, bool isMemoryNode = false)
      : cacheConfig(cacheConfig), machineNR(machineNR), memoryNR(memoryNR), dsmSize(dsmSize), isMemoryNode(isMemoryNode) {}
};

#endif /* __CONFIG_H__ */
