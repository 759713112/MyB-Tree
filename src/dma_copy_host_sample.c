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

#include "dma_copy_sample.h"
DOCA_LOG_REGISTER(DMA_COPY_HOST);

#define SLEEP_IN_NANOS (10 * 1000)	/* Sample the job every 10 microseconds  */
#define RECV_BUF_SIZE 256		/* Buffer which contains config information */

/*
 * Run DOCA DMA Host copy sample
 *
 * @pcie_addr [in]: Device PCI address
 * @src_buffer [in]: Source buffer to copy
 * @src_buffer_size [in]: Buffer size
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_name [in]: Buffer info file path
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
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

	// DOCA_LOG_INFO("Please copy %s and %s to the DPU and run DMA Copy DPU sample before closing", export_desc_file_path, buffer_info_file_name);

	// /* Saves the export desc and buffer info to files, it is the user responsibility to transfer them to the dpu */
	// result = save_config_info_to_files(export_desc, export_desc_len, src_buffer, src_buffer_size,
	// 				   export_desc_file_path, buffer_info_file_name);
	// if (result != DOCA_SUCCESS) {
	// 	host_destroy_core_objects(&state);
	// 	return result;
	// }

	// /* Wait until DMA copy on the DPU is over */
	// while (!is_dma_done_on_dpu)
	// 	sleep(1);

	/* Destroy all relevant DOCA core objects */
	// host_destroy_core_objects(&state);

	return result;
}

/*
 * Run DOCA DMA DPU copy sample
 *
 * @export_desc_file_path [in]: Export descriptor file path
 * @buffer_info_file_path [in]: Buffer info file path
 * @pcie_addr [in]: Device PCI address
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
dma_copy_dpu(struct program_core_objects *state, const char *export_desc, size_t export_desc_len, 
			void *remote_addr, size_t remote_buffer_size, const char *pcie_addr)
{
	
	struct doca_event event = {0};
	struct doca_dma_job_memcpy dma_job;
	struct doca_dma *dma_ctx;
	struct doca_buf *src_doca_buf;
	struct doca_buf *dst_doca_buf;
	struct doca_mmap *remote_mmap;
	doca_error_t result;
	uint32_t max_bufs = 2;

	char *dpu_buffer;
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = SLEEP_IN_NANOS,
	};

	result = doca_dma_create(&dma_ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to create DMA engine: %s", doca_get_error_string(result));
		return result;
	}

	state->ctx = doca_dma_as_ctx(dma_ctx);

	result = open_doca_device_with_pci(pcie_addr, &dma_jobs_is_supported, &state->dev);
	if (result != DOCA_SUCCESS) {
		doca_dma_destroy(dma_ctx);
		return result;
	}

	result = init_core_objects(&state, WORKQ_DEPTH, max_bufs);
	if (result != DOCA_SUCCESS) {
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Copy all relevant information into local buffers */
	// save_config_info_to_buffers(export_desc_file_path, buffer_info_file_path, export_desc, &export_desc_len,
	// 			    &remote_addr, &remote_addr_len);

	/* Copy the entire host buffer */
	
	dpu_buffer = (char *)malloc(remote_buffer_size);
	if (dpu_buffer == NULL) {
		DOCA_LOG_ERR("Failed to allocate buffer memory");
		dma_cleanup(&state, dma_ctx);
		return DOCA_ERROR_NO_MEMORY;
	}

	result = doca_mmap_set_memrange(state->dst_mmap, dpu_buffer, remote_buffer_size);
	if (result != DOCA_SUCCESS) {
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}
	result = doca_mmap_start(state->dst_mmap);
	if (result != DOCA_SUCCESS) {
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Create a local DOCA mmap from exported data */
	result = doca_mmap_create_from_export(NULL, (const void *)export_desc, export_desc_len, state->dev,
					   &remote_mmap);
	if (result != DOCA_SUCCESS) {
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state->buf_inv, remote_mmap, remote_addr, remote_buffer_size,
						&src_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing remote buffer: %s",
			     doca_get_error_string(result));
		doca_mmap_destroy(remote_mmap);
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DOCA buffer for each address range */
	result = doca_buf_inventory_buf_by_addr(state->buf_inv, state->dst_mmap, dpu_buffer, remote_buffer_size,
						&dst_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA buffer representing destination buffer: %s",
			     doca_get_error_string(result));
		doca_buf_refcount_rm(src_doca_buf, NULL);
		doca_mmap_destroy(remote_mmap);
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Construct DMA job */
	dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
	dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
	dma_job.base.ctx = state->ctx;
	dma_job.dst_buff = dst_doca_buf;
	dma_job.src_buff = src_doca_buf;

	/* Set data position in src_buff */
	result = doca_buf_set_data(src_doca_buf, remote_addr, remote_buffer_size);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set data for DOCA buffer: %s", doca_get_error_string(result));
		return result;
	}


	/* Enqueue DMA job */
	result = doca_workq_submit(state->workq, &dma_job.base);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(result));
		doca_buf_refcount_rm(dst_doca_buf, NULL);
		doca_buf_refcount_rm(src_doca_buf, NULL);
		doca_mmap_destroy(remote_mmap);
		free(dpu_buffer);
		dma_cleanup(&state, dma_ctx);
		return result;
	}

	/* Wait for job completion */
	while ((result = doca_workq_progress_retrieve(state->workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
		DOCA_ERROR_AGAIN) {
		nanosleep(&ts, &ts);
	}

	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to retrieve DMA job: %s", doca_get_error_string(result));
		return result;
	}

	/* event result is valid */
	result = (doca_error_t)event.result.u64;
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("DMA job event returned unsuccessfully: %s", doca_get_error_string(result));
		return result;
	}

	DOCA_LOG_INFO("Remote DMA copy was done Successfully");
	dpu_buffer[remote_buffer_size - 1] = '\0';
	DOCA_LOG_INFO("Memory content: %s", dpu_buffer);

	if (doca_buf_refcount_rm(src_doca_buf, NULL) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove DOCA source buffer reference count");

	if (doca_buf_refcount_rm(dst_doca_buf, NULL) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove DOCA destination buffer reference count");

	/* Destroy remote memory map */
	if (doca_mmap_destroy(remote_mmap) != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to destroy remote memory map");

	/* Inform host that DMA operation is done */
	DOCA_LOG_INFO("Host sample can be closed, DMA copy ended");

	/* Clean and destroy all relevant objects */
	dma_cleanup(&state, dma_ctx);

	free(dpu_buffer);

	return result;
}
