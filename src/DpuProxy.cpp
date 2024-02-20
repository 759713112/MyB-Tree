
#include "DpuProxy.h"
#include "Directory.h"
#include "HugePageAlloc.h"
#include "DSDpuKeeper.h"

#include <algorithm>


thread_local DmaConnect DpuProxy::dmaCon;
thread_local ibv_cq* DpuProxy::completeQueue;

DpuProxy *DpuProxy::getInstance(const DSMConfig &conf) {
	static DpuProxy dsm(conf);
	return &dsm;
}

DpuProxy::DpuProxy(const DSMConfig &conf) : DSM(conf) {
	remoteInfo = new RemoteConnection[1];	
	computeInfo = new RemoteConnection[conf.machineNR];
	createContext(&ctx);
	
	Debug::notifyInfo("number of servers (colocated MN/CN): %d", conf.machineNR);

	int dpuConPerThread = (conf.machineNR * MAX_APP_THREAD + MAX_DPU_THREAD - 1) / MAX_DPU_THREAD;
	int dpuConID = 0;	
	for (int i = 0; i < MAX_DPU_THREAD; ++i) {
		completeQueues[i] = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
		for (int j = 0; j < dpuConPerThread; j++) {
			dpuCons[dpuConID] = new DpuConnection(ctx, completeQueues[i], APP_MESSAGE_NR, 1088, 64, dpuConID);
			dpuConID++;
		}
	}
	Debug::notifyInfo("qp num %d", dpuConID);
	
	keeper = new DSDpuKeeper(&ctx, dpuCons, remoteInfo, computeInfo, conf.machineNR);

	init_dma_state();
	
	myNodeID = keeper->getMyNodeID();
	auto t =  new std::thread(std::bind(&DpuProxy::catch_root_change, this));
	// keeper->barrier("DpuProxy-init");
}

DpuProxy::~DpuProxy() {}

void DpuProxy::registerThread() {

  static bool has_init[MAX_DPU_THREAD];

  if (thread_id != -1)
    return;

  thread_id = appID.fetch_add(1);

  completeQueue = completeQueues[thread_id];

  if (!has_init[thread_id]) {
    has_init[thread_id] = true;
  }

  dmaCon.init(&dmaConCtx, define::kMaxCoro * 2);
}

void DpuProxy::rpc_call_dir(const RawMessage &m, uint16_t node_id,
                    uint16_t dir_id) {

    auto buffer = (RawMessage *)iCon->message->getSendPool();

    memcpy(buffer, &m, sizeof(RawMessage));
    buffer->node_id = NODE_ID_FOR_DPU;
    buffer->app_id = thread_id;

    iCon->sendMessage2Dir(buffer, node_id, dir_id);
}

void DpuProxy::read(char *buffer, GlobalAddress gaddr, size_t size, bool signal,
               CoroContext *ctx) {
	if (ctx == nullptr) {
	rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
				remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
				iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], signal);
	} else {
	rdmaRead(iCon->data[0][gaddr.nodeID], (uint64_t)buffer,
				remoteInfo[gaddr.nodeID].dsmBase + gaddr.offset, size,
				iCon->cacheLKey, remoteInfo[gaddr.nodeID].dsmRKey[0], true,
				ctx->coro_id);
	(*ctx->yield)(*ctx->master);
	}
}

void DpuProxy::read_sync(char *buffer, GlobalAddress gaddr, size_t size,
                    CoroContext *ctx) {
#ifndef AARCH64
  read(buffer, gaddr, size, true, ctx);
  if (ctx == nullptr) {
    ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
  }
#else
#endif
}

void* DpuProxy::readByDmaFixedSize(GlobalAddress gaddr, CoroContext *ctx) {
	return this->dpuCache->get(gaddr.offset, ctx);
}

void* DpuProxy::readWithoutCache(GlobalAddress gaddr, CoroContext *ctx) {

	auto buffer = local_buffer + (this->getMyThreadID() * 16 + ctx->coro_id) * 1024;
	dmaCon.readByDma(buffer, gaddr.offset, 1024, ctx);

	return buffer;
}

void DpuProxy::init_dma_state() {
	size_t local_buffer_size = (DPU_CACHE_INTERNAL_PAGE_NUM + MAX_DPU_THREAD * 16) * sizeof(InternalPage);
	local_buffer = hugePageAlloc(local_buffer_size);
#ifndef AARCH64
	auto result = dmaConCtx.connect(this->keeper->get_dma_export_desc(), this->keeper->get_dma_export_desc_len(), 
				(void*)this->remoteInfo[0].dsmBase, conf.dsmSize * define::GB, local_buffer, local_buffer_size,
				DMA_PCIE_ADDR_ON_HOST);
#else
	auto result = dmaConCtx.connect(this->keeper->get_dma_export_desc(), this->keeper->get_dma_export_desc_len(), 
		(void*)this->remoteInfo[0].dsmBase, conf.dsmSize * define::GB, local_buffer, local_buffer_size, DMA_PCIE_ADDR_ON_DPU);
#endif

	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed DMA init: %s", doca_get_error_string(result));
		exit(-1);
	} else {
		Debug::notifyInfo("DMA connect success");
	}

	this->dpuCache = new DpuCacheMultiCon(
		[](void *buffer, uint64_t addr, size_t size, CoroContext *ctx) {
			dmaCon.readByDma(buffer, addr, size, ctx);
		}, local_buffer + (MAX_DPU_THREAD * 16) * sizeof(InternalPage), DPU_CACHE_INTERNAL_PAGE_NUM * sizeof(InternalPage), kInternalPageSize);
}


bool DpuProxy::poll_dma_cq_once(uint64_t &next_coro_id) {
	return dmaCon.poll_dma_cq(next_coro_id);
}

#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
extern GlobalAddress g_root_ptr;
extern int g_root_level;
extern bool enable_cache;

std::atomic<InternalPage*> root_node;
void DpuProxy::catch_root_change() {
  std::cout << "start catch broadcast" << std::endl;
  int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Error creating socket\n";
        exit(-1);
    }

    int broadcastEnable = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1) {
        std::cerr << "Error setting socket options\n";
        close(udpSocket);
        exit(-1);
    }

    struct sockaddr_in serverAddress, clientAddress;
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345); // Port number
    serverAddress.sin_addr.s_addr = inet_addr("192.168.6.255");
    int val = 1;
    setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    setsockopt(udpSocket, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
    if (bind(udpSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket\n";
        close(udpSocket);
        exit(-1);
    }
	DmaConnectCtx dmaCtx;
	size_t local_buffer_size = 2 * sizeof(InternalPage);
	void* local_buffer = malloc(local_buffer_size);
#ifndef AARCH64
	auto result = dmaCtx.connect(this->keeper->get_dma_export_desc(), this->keeper->get_dma_export_desc_len(), 
				(void*)this->remoteInfo[0].dsmBase, conf.dsmSize * define::GB, local_buffer, local_buffer_size,
				DMA_PCIE_ADDR_ON_HOST);
#else
	auto result = dmaCtx.connect(this->keeper->get_dma_export_desc(), this->keeper->get_dma_export_desc_len(), 
		(void*)this->remoteInfo[0].dsmBase, conf.dsmSize * define::GB, local_buffer, local_buffer_size, DMA_PCIE_ADDR_ON_DPU);
#endif

	dmaCon.init(&dmaCtx, 2);
    char buffer[1024];
	int flag = 0;
    while (true) {
      
      socklen_t clientAddressLength = sizeof(clientAddress);
      ssize_t receivedBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddress, &clientAddressLength);
      if (receivedBytes == -1 || receivedBytes != sizeof(RawMessage)) {
          std::cerr << "Error receiving message\n";
          close(udpSocket);
          exit(-1);
      }
      RawMessage* m = (RawMessage*)buffer;
      if (g_root_level < m->level) {
          g_root_ptr = m->addr;
          g_root_level = m->level;
          if (g_root_level >= 3) {
            enable_cache = true;
          }
		//   dmaCon
		InternalPage* new_buffer = (InternalPage*)(local_buffer + (flag * sizeof(InternalPage)));
		dmaCon.readByDma((void*) new_buffer, g_root_ptr.offset, 1024);
		if(!new_buffer->check_consistent()) {
			std::cout << "Read root error" << g_root_ptr << std::endl;
			exit(-1);
		}
		root_node.store(new_buffer);
		
		flag ^= 1;	
        std::cout << "Received change root to" << g_root_ptr << std::endl;
      }
    }
}