/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */
#include <doca_argp.h>
#include <doca_dev.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>

#include "DmaDpu.h"
#include "Common.h"

#define SLEEP_IN_NANOS (1000)	/* Sample the job every 10 microseconds  */
#define RECV_BUF_SIZE 256		/* Buffer which contains config information */

doca_error_t
dma_copy_host(const char *pcie_addr, void *src_buffer, size_t src_buffer_size,
		    const void **export_desc, size_t *export_desc_len)
{
	struct program_core_objects state = {0};
	doca_error_t result;

	/* Open the relevant DOCA device */
	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state.dev);
	if (result != DOCA_SUCCESS)
		return result;

	/* Init all DOCA core objects */
	result = host_init_core_objects(&state);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	/* Allow exporting the mmap to DPU for read only operations */
	result = doca_mmap_set_permissions(state.src_mmap, DOCA_ACCESS_DPU_READ_ONLY);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	/* Populate the memory map with the allocated memory */
	result = doca_mmap_set_memrange(state.src_mmap, src_buffer, src_buffer_size);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}
	result = doca_mmap_start(state.src_mmap);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	/* Export DOCA mmap to enable DMA on Host*/
	result = doca_mmap_export_dpu(state.src_mmap, state.dev, export_desc, export_desc_len);
	if (result != DOCA_SUCCESS) {
		host_destroy_core_objects(&state);
		return result;
	}

	return result;
}


doca_error_t
dma_copy_dpu(struct program_core_objects *state, const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr)
{
	
	// struct doca_dma *dma_ctx;

	// struct doca_mmap *remote_mmap;
	// doca_error_t result;
	// uint32_t max_bufs = 2;


	// struct timespec ts = {
	// 	.tv_sec = 0,
	// 	.tv_nsec = SLEEP_IN_NANOS,
	// };

	// result = doca_dma_create(&dma_ctx);
	// if (result != DOCA_SUCCESS) {
	// 	//DOCA_LOG_ERR("Unable to create DMA engine: %s", doca_get_error_string(result));
	// 	return result;
	// }

	// state->ctx = doca_dma_as_ctx(dma_ctx);
	// if (state->ctx == NULL) {
	// 	return DOCA_ERROR_UNKNOWN;
	// }

	// result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state->dev);
	// if (result != DOCA_SUCCESS) {
	// 	doca_dma_destroy(dma_ctx);
	// 	return result;
	// }

	// result = init_core_objects(state, WORKQ_DEPTH, max_bufs);
	// if (result != DOCA_SUCCESS) {
	// 	dma_cleanup(state, dma_ctx);
	// 	return result;
	// }

	// /* Copy all relevant information into local buffers */
	// // save_config_info_to_buffers(export_desc_file_path, buffer_info_file_path, export_desc, &export_desc_len,
	// // 			    &remote_addr, &remote_addr_len);

	// /* Copy the entire host buffer */
	


	// /* Create a local DOCA mmap from exported data */
	// result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, state->dev,
	// 				   &remote_mmap);
	// if (result != DOCA_SUCCESS) {
	// 	dma_cleanup(state, dma_ctx);
	// 	return result;
	// }

	// /* Construct DOCA buffer for each address range */
	// result = doca_buf_inventory_buf_by_addr(state->buf_inv, remote_mmap, remote_addr, remote_buffer_size,
	// 					src_doca_buf);
	// if (result != DOCA_SUCCESS) {
	// 	//DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s",
	// 		    //  doca_get_error_string(result));
	// 	doca_mmap_destroy(remote_mmap);
	// 	free(dpu_buffer);
	// 	dma_cleanup(state, dma_ctx);
	// 	return result;
	// }
	
	return DOCA_SUCCESS;
}



DmaConnectCtx::DmaConnectCtx() {
  memset(&state, 0, sizeof(program_core_objects));
  src_doca_buf = nullptr;
  ctx = nullptr;
  remote_mmap = nullptr;
}

DmaConnectCtx::~DmaConnectCtx() {

  if (remote_mmap != nullptr) {
    doca_mmap_destroy(remote_mmap);
  }
  if (src_doca_buf != nullptr) {
    doca_buf_refcount_rm(src_doca_buf, NULL);
  }
  if (ctx != nullptr) {
    dma_cleanup(&state, ctx);
  }


}


doca_error_t DmaConnectCtx::connect(const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr) {
  this->remote_addr = remote_addr;
  this->remote_buffer_size = remote_buffer_size;
  doca_error_t result;
	uint32_t max_bufs = 2;

  result = doca_argp_init("doca_dma_copy_dpu", nullptr);
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

	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};

	result = doca_dma_create(&this->ctx);
	if (result != DOCA_SUCCESS) {
		Debug::notifyInfo("Unable to create DMA engine: %s", doca_get_error_string(result));
		return result;
	}

	this->state.ctx = doca_dma_as_ctx(this->ctx);
	if (this->state.ctx == NULL) {
		return DOCA_ERROR_UNKNOWN;
	}

	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state.dev);
	if (result != DOCA_SUCCESS) {
		doca_dma_destroy(this->ctx);
		return result;
	}

	result = init_core_objects(&this->state, WORKQ_DEPTH, max_bufs);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&this->state, this->ctx);
		return result;
	}

	/* Create a local DOCA mmap from exported data */
	result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, this->state.dev,
					   &remote_mmap);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&this->state, this->ctx);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(this->state.buf_inv, remote_mmap, remote_addr, remote_buffer_size,
						&this->src_doca_buf);
	if (result != DOCA_SUCCESS) {
		Debug::notifyInfo("Unable to acquire DOCA buffer representing remote buffer: %s",
			     doca_get_error_string(result));
		doca_mmap_destroy(remote_mmap);
		dma_cleanup(&this->state, this->ctx);
		return result;
	}
	
	return result;
}

#define SLEEP_IN_NANOS (10 * 1000)
void* DmaConnectCtx::mmapSetMemrange(char* buffer, size_t size) {
	auto result = doca_mmap_set_memrange(state.dst_mmap, buffer, 1024);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Unable to set mmap memrange: %s",
					doca_get_error_string(result));
		return nullptr;
	}

	result = doca_mmap_start(state.dst_mmap);
	if (result != DOCA_SUCCESS) {
    	Debug::notifyError("Unable to start doca mmap: %s",
			    doca_get_error_string(result));
    	return nullptr;
	}
	struct doca_buf *dst_doca_buf;
	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state.buf_inv, state.dst_mmap, buffer, 1024,
						&dst_doca_buf);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Unable to acquire DOCA buffer representing destination buffer: %s",
			      doca_get_error_string(result));
		return nullptr;
	}
	return dst_doca_buf;
}
#include "Timer.h"
void DmaConnectCtx::readByDma(char* buffer, uint64_t offset, size_t size, bool signal, void* dst_doca_buf_) {

  	struct doca_event event = {0};
	struct doca_dma_job_memcpy dma_job;

	struct doca_buf *dst_doca_buf = (doca_buf *)dst_doca_buf_;


	doca_buf_reset_data_len(dst_doca_buf);
	// Debug::notifyError("1 %d", t.end());
	/* Construct DMA job */
	dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
	dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
	dma_job.base.ctx = state.ctx;
	dma_job.dst_buff = dst_doca_buf;
	dma_job.src_buff = src_doca_buf;

	/* Set data position in src_buff */
	auto result = doca_buf_set_data(src_doca_buf, remote_addr + offset, 1024);
		// Debug::notifyError("2 %d", t.end());
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to set data for DOCA buffer: %s", doca_get_error_string(result));
		return;
	}

	Timer t;
	t.begin();
	/* Enqueue DMA job */
	result = doca_workq_submit(state.workq, &dma_job.base);\
		// Debug::notifyError("3 %d", t.end());
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to submit DMA job: %s", doca_get_error_string(result));
		return;
	}

  struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};

	/* Wait for job completion */
	while ((result = doca_workq_progress_retrieve(state.workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
		DOCA_ERROR_AGAIN) {
		// nanosleep(&ts, &ts);
	}
	Debug::notifyError("4 %d", t.end());
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to retrieve DMA job: %s", doca_get_error_string(result));
		return;
	}

	/* event result is valid */
	result = (doca_error_t)event.result.u64;
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("DMA job event returned unsuccessfully: %s", doca_get_error_string(result));
		return;
	}

	Debug::notifyInfo("Remote DMA copy was done Successfully");
	buffer[1024 - 1] = '\0';
	Debug::notifyInfo("Memory content: %s", buffer);

}
