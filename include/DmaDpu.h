#ifndef __DMA_DPU_H__
#define __DMA_DPU_H__
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <doca_dma.h>
#include <doca_error.h>

#include <doca_mmap.h>
#include <doca_buf_inventory.h>
#include "Common.h"

extern "C" {
	#include "common.h"
  #include "dma_common.h"
}


class DmaConnectCtx {
public:
  DmaConnectCtx();
  ~DmaConnectCtx();
  doca_error_t connect(const char *export_desc, size_t export_desc_len, 
	  void *remote_addr, size_t remote_buffer_size, void *local_buffer, size_t local_buffer_size, 
    const char *pcie_addr);
  doca_error_t createWorkQueue(uint32_t workq_depth, struct doca_workq **workq);
  doca_error_t getDstDocaBuf(doca_buf **dst_doca_buf);
  doca_error_t getSrcDocaBuf(doca_buf **src_doca_buf);

  void* getRemoteAddr() const { return remote_addr; }
  doca_ctx* getDocaCtx() const { return state.ctx; }
private:
  struct program_core_objects state;  //公有
  struct doca_dma *dma;
  struct doca_mmap *remote_mmap;

  void *remote_addr;
  size_t remote_buffer_size;

  void *local_buffer;
  size_t local_buffer_size;
};

class DmaConnect {
public:
  DmaConnect() {};
  doca_error init(DmaConnectCtx *dmaConCtx, uint32_t workq_depth);
  void readByDma(void* buffer, uint64_t offset, size_t size, CoroContext *ctx= nullptr);
private:
  DmaConnectCtx *dmaConCtx;
  void* remote_addr;
  struct doca_ctx *doca_ctx;
  //线程私有
  struct doca_workq *workq;		/* doca work queue */
  //协程私有
  struct doca_buf* src_doca_buf[define::kMaxCoro];
  struct doca_buf* dst_doca_buf[define::kMaxCoro];


};



doca_error_t
dma_copy_host(const char *pcie_addr, void *src_buffer, size_t src_buffer_size,
			 const void **export_desc, size_t *export_desc_len);

doca_error_t
dma_copy_dpu(struct program_core_objects *state, const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr);


#endif