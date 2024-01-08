
#include "DSMemory.h"
#include "Directory.h"
#include "HugePageAlloc.h"

#include <algorithm>
#include <iostream>

#include <doca_argp.h>
#include <doca_dev.h>
extern "C" {
	#include "dma_common.h"
	#include "dma_copy_sample.h"
}

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
  dpuConnectInfo = new RemoteConnection;

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
                                conf.machineNR, remoteInfo, dpuConnectInfo, true);
  }
  // clear up first chunk
  memset((char *)baseAddr, 0, define::kChunkSize);
  init_dma_host_args();

  
  keeper = new DSMemoryKeeper(dirCon, remoteInfo, dpuConnectInfo, conf.machineNR, dma_export_desc, dma_export_desc_len);

  myNodeID = keeper->getMyNodeID();

  Debug::notifyInfo("number of threads on memory node: %d", NR_DIRECTORY);
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    dirAgent[i] =
        new Directory(dirCon[i], remoteInfo, conf.machineNR, i, myNodeID);
  }

  keeper->barrier("DSM-init");
}

DSMemory::~DSMemory() {}

void DSMemory::init_dma_host_args() {
	doca_error_t result;
	int exit_status = EXIT_FAILURE;

#ifndef DOCA_ARCH_HOST
	DOCA_LOG_ERR("Sample can run only on the Host");
	goto sample_exit;
#endif

	result = doca_argp_init("doca_dma_copy_host", nullptr);
	if (result != DOCA_SUCCESS) {
    Debug::notifyInfo("Failed to init ARGP resources: %s", doca_get_error_string(result));
		exit(-1);
	}
	result = register_dma_params(true);
	if (result != DOCA_SUCCESS) {
    Debug::notifyInfo("Failed to register DMA sample parameters: %s", doca_get_error_string(result));
		exit(-1);
	}
	result = doca_argp_start(0, nullptr);
	if (result != DOCA_SUCCESS) {
		Debug::notifyInfo("Failed to parse sample input: %s", doca_get_error_string(result));
		exit(-1);
	}

	result = dma_copy_host(DMA_PCIE_ADDR, (void*)this->baseAddr, conf.dsmSize * define::GB, &dma_export_desc, &dma_export_desc_len);
	if (result != DOCA_SUCCESS) {
		Debug::notifyInfo("dma_copy_host() encountered an error: %s", doca_get_error_string(result));
		exit(-1);
	}
}
