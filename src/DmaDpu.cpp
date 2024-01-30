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
#include "Timer.h"

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
	return DOCA_SUCCESS;
}



DmaConnectCtx::DmaConnectCtx() {
  memset(&state, 0, sizeof(program_core_objects));
  dma = nullptr;
  remote_mmap = nullptr;
}

DmaConnectCtx::~DmaConnectCtx() {
  if (remote_mmap != nullptr) {
    doca_mmap_destroy(remote_mmap);
  }

  if (dma != nullptr) {
    dma_cleanup(&state, dma);
  }
}


doca_error_t DmaConnectCtx::connect(
		const char *export_desc, size_t export_desc_len, 
		void *remote_addr, size_t remote_buffer_size, 
		void *local_buffer, size_t local_buffer_size,
		const char *pcie_addr) {
	this->remote_addr = remote_addr;
	this->remote_buffer_size = remote_buffer_size;
	this->local_buffer = local_buffer;
	this->local_buffer_size = local_buffer_size; 
	doca_error_t result;


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

	result = doca_dma_create(&this->dma);
	if (result != DOCA_SUCCESS) {
		Debug::notifyInfo("Unable to create DMA engine: %s", doca_get_error_string(result));
		return result;
	}

	this->state.ctx = doca_dma_as_ctx(this->dma);
	if (this->state.ctx == NULL) {
		return DOCA_ERROR_UNKNOWN;
	}

	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state.dev);
	if (result != DOCA_SUCCESS) {
		doca_dma_destroy(this->dma);
		return result;
	}
	result = init_core_objects(&this->state, WORKQ_DEPTH, MAX_DOCA_BUFS);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&this->state, this->dma);
		return result;
	}

	/* Create a local DOCA mmap from exported data */
	result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, this->state.dev,
						&remote_mmap);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&this->state, this->dma);
		return result;
	}


	result = doca_mmap_set_memrange(state.dst_mmap, local_buffer, local_buffer_size);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Unable to set mmap memrange: %s",
					doca_get_error_string(result));

	}

	result = doca_mmap_start(state.dst_mmap);
	if (result != DOCA_SUCCESS) {
    	Debug::notifyError("Unable to start doca mmap: %s",
			    doca_get_error_string(result));
	}
	return result;
}

doca_error_t DmaConnectCtx::createWorkQueue(uint32_t workq_depth, struct doca_workq **workq) {
	auto res = doca_workq_create(workq_depth, workq);
	if (res != DOCA_SUCCESS) {
		 Debug::notifyInfo("Unable to create work queue: %s", doca_get_error_string(res));
		return res;
	}
	res = doca_ctx_workq_add(this->state.ctx, *workq);
	if (res != DOCA_SUCCESS) {
		 Debug::notifyInfo("Unable to register work queue with context: %s", doca_get_error_string(res));
		doca_workq_destroy(*workq);
	}
	return res;
}
doca_error_t DmaConnectCtx::getDstDocaBuf(doca_buf **dst_doca_buf) {

	/* Construct DOCA buffer for each address range */
	auto result = doca_buf_inventory_buf_by_addr(state.buf_inv, state.dst_mmap, local_buffer, 4096,
						dst_doca_buf);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Unable to acquire DOCA buffer representing destination buffer: %s",
			      doca_get_error_string(result));
		return result;
	}
	return result;
}

doca_error_t DmaConnectCtx::getSrcDocaBuf(doca_buf **src_doca_buf) {

	/* Construct DOCA buffer for each address range */
	auto result = doca_buf_inventory_buf_by_addr(state.buf_inv, remote_mmap, remote_addr, remote_buffer_size,
						src_doca_buf);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Unable to acquire DOCA buffer representing remote buffer: %s",
			      doca_get_error_string(result));
		return result;
	}
	return result;
}

doca_error DmaConnect::init(DmaConnectCtx *dmaConCtx, uint32_t workq_depth) {

	this->doca_ctx = dmaConCtx->getDocaCtx();
	this->remote_addr = dmaConCtx->getRemoteAddr();

	auto res = dmaConCtx->createWorkQueue(workq_depth, &this->workq);
	if (res != DOCA_SUCCESS) {
		return res;
	}

	for (uint16_t i = 0; i < define::kMaxCoro; i++) {
		/* Construct DOCA buffer for each address range */
		res = dmaConCtx->getSrcDocaBuf(&this->src_doca_buf[i]);
		if (res != DOCA_SUCCESS) {
		return res;
		}
		res = dmaConCtx->getDstDocaBuf(&this->dst_doca_buf[i]);
		if (res != DOCA_SUCCESS) {
		return res;
		}
	}
	return res;
}

void DmaConnect::readByDma(void *buffer, uint64_t offset, size_t size, CoroContext *ctx) {


	struct doca_dma_job_memcpy dma_job;
	auto result = doca_buf_set_data(this->src_doca_buf[ctx->coro_id], (void*)remote_addr + offset, size);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to set data for SRC DOCA buffer: %s", doca_get_error_string(result));
		return;
	}
	result = doca_buf_set_data(this->dst_doca_buf[ctx->coro_id], buffer, 0);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to set data for DST DOCA buffer: %s", doca_get_error_string(result));
		return;
	}
	/* Construct DMA job */
	dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
	dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
	dma_job.base.ctx = this->doca_ctx;
	dma_job.dst_buff = this->dst_doca_buf[ctx->coro_id];
	dma_job.src_buff = this->src_doca_buf[ctx->coro_id];
	dma_job.base.user_data.u64 = ctx->coro_id; 

	/* Enqueue DMA job */
	result = doca_workq_submit(this->workq, &dma_job.base);
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to submit DMA job: %s", doca_get_error_string(result));
		return;
	}
	if (ctx != nullptr) {
		(*ctx->yield)(*ctx->master);
	}
	return;
}

bool DmaConnect::poll_dma_cq(uint64_t &id) {
	struct doca_event event = {0};
	/* Wait for job completion */
	doca_error_t result = doca_workq_progress_retrieve(this->workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE);
	if (result == DOCA_ERROR_AGAIN) {
		return false;
	}
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("Failed to retrieve DMA job: %s", doca_get_error_string(result));
		return false;
	}
	/* event result is valid */
	result = (doca_error_t)event.result.u64;
	if (result != DOCA_SUCCESS) {
		Debug::notifyError("DMA job event returned unsuccessfully: %s", doca_get_error_string(result));
		return false;
	}
	id = event.user_data.u64;
	return true;
}