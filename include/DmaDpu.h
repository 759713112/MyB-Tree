
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

extern "C" {
	#include "common.h"
  #include "dma_common.h"
}


class DmaConnectCtx {
public:
  DmaConnectCtx();
  ~DmaConnectCtx();
  doca_error_t connect(const char *export_desc, size_t export_desc_len, 
		void *remote_addr, size_t remote_buffer_size, const char *pcie_addr);
  void readByDma(char* buffer, uint64_t offset, size_t size, bool signal, void*);
  void* mmapSetMemrange(char* buffer, size_t size);
private:
  struct program_core_objects state;
  struct doca_buf *src_doca_buf;
  struct doca_dma *ctx;
  struct doca_mmap *remote_mmap;
  size_t remote_buffer_size;
  void *remote_addr;
 
};

doca_error_t
dma_copy_host(const char *pcie_addr, void *src_buffer, size_t src_buffer_size,
			 const void **export_desc, size_t *export_desc_len);

doca_error_t
dma_copy_dpu(struct program_core_objects *state, const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr);


