
#include "DpuProxy.h"
#include "Directory.h"
#include "HugePageAlloc.h"
#include "DSDpuKeeper.h"

#include <algorithm>


thread_local DmaConnect DpuProxy::dmaCon;

DpuProxy *DpuProxy::getInstance(const DSMConfig &conf) {
	static DpuProxy dsm(conf);
	return &dsm;
}

DpuProxy::DpuProxy(const DSMConfig &conf)
    : DSM(conf) {

	remoteInfo = new RemoteConnection[1];
	computeInfo = new RemoteConnection[conf.machineNR];


	Debug::notifyInfo("number of servers (colocated MN/CN): %d", conf.machineNR);
	for (int i = 0; i < MAX_DPU_THREAD; ++i) {
	thCon[i] =  new ThreadConnection(i, (void *)cache.data, cache.size * define::GB, 1, remoteInfo);
	// auto rpc_cq = ibv_create_cq(hostCon[i]->ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
	// dpuCon[i] = new DpuConnection(hostCon[i]->ctx, rpc_cq, APP_MESSAGE_NR);
	// dpuCon[i]->initRecv();
	}


	keeper = new DSDpuKeeper(thCon, remoteInfo, computeInfo, conf.machineNR);

	init_dma_state();
	
	myNodeID = keeper->getMyNodeID();
	keeper->barrier("DpuProxy-init");
}

DpuProxy::~DpuProxy() {}

void DpuProxy::registerThread() {

  static bool has_init[MAX_DPU_THREAD];

  if (thread_id != -1)
    return;

  thread_id = appID.fetch_add(1);
  thread_tag = thread_id + (((uint64_t)this->getMyNodeID()) << 32) + 1;

  iCon = thCon[thread_id];
  this->init_dma_state();

  if (!has_init[thread_id]) {
    has_init[thread_id] = true;
  }

  rdma_buffer = (char *)cache.data + thread_id * 12 * define::MB;

  for (int i = 0; i < define::kMaxCoro; ++i) {
    rbuf[i].set_buffer(rdma_buffer + i * define::kPerCoroRdmaBuf);
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


void DpuProxy::readByDma(void *buffer, uint64_t addr, size_t size, CoroContext *ctx) {
	dmaCon.readByDma(buffer, addr, size, ctx);
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
	}
	using namespace std::placeholders;
	dmaCon.init(&dmaConCtx, 10);
	CoroContext ctx;
	ctx.coro_id = 2;
	this->dpuCache = new DpuCacheMultiCon(std::bind(&DpuProxy::readByDma, this, _1, _2, _3, _4), 
							local_buffer, local_buffer_size, 1024);
	char* data = (char*)dpuCache->get(0, &ctx);
	sleep(5);
	Debug::notifyInfo("read1111 %s", data);
}
