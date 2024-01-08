
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <doca_dma.h>
#include <doca_error.h>
#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>

#include "dma_common.h"

doca_error_t
dma_copy_host(const char *pcie_addr, void *src_buffer, size_t src_buffer_size,
			 const void **export_desc, size_t *export_desc_len);

doca_error_t
dma_copy_dpu(struct program_core_objects *state, const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr);
