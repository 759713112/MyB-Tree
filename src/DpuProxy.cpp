
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

	int dpuConPerThread = std::max((conf.machineNR * MAX_APP_THREAD) / MAX_DPU_THREAD, 1u);
	int dpuConID = 0;	
	for (int i = 0; i < MAX_DPU_THREAD; ++i) {
		completeQueues[i] = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
		for (int j = 0; j < dpuConPerThread; j++) {
			dpuCons[dpuConID] = new DpuConnection(ctx, completeQueues[i], APP_MESSAGE_NR, 1024, 1024, dpuConID);
			dpuConID++;
		}
	}
	Debug::notifyInfo("qp num %d", dpuConID);
	
	keeper = new DSDpuKeeper(&ctx, dpuCons, remoteInfo, computeInfo, conf.machineNR);

	init_dma_state();
	
	myNodeID = keeper->getMyNodeID();

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

//   rdma_buffer = (char *)cache.data + thread_id * 12 * define::MB;

//   for (int i = 0; i < define::kMaxCoro; ++i) {
//     rbuf[i].set_buffer(rdma_buffer + i * define::kPerCoroRdmaBuf);
//   }

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

void DpuProxy::init_dma_state() {
	size_t local_buffer_size = DPU_CACHE_INTERNAL_PAGE_NUM * sizeof(InternalPage);
	void* local_buffer = hugePageAlloc(local_buffer_size);
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
		}, local_buffer, local_buffer_size, kInternalPageSize);
}


bool DpuProxy::poll_dma_cq_once(uint64_t &next_coro_id) {
	return dmaCon.poll_dma_cq(next_coro_id);
}