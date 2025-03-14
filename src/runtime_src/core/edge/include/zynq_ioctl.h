/*
 * A GEM style CMA backed memory manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Min Ma       <min.ma@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * DOC: A GEM style driver for Xilinx edge based accelerators
 * This file defines ioctl command codes and associated structures for interacting with
 * *zocl* driver for Xilinx FPGA platforms(Zynq/ZynqMP/Versal).
 *
 * Accelerator memory allocation is modeled as buffer objects (bo). zocl
 * supports both SMMU based shared virtual memory and CMA based shared physical memory
 * between PS and PL. zocl also supports memory management of PL-DDRs and PL-BRAMs.
 * PL-DDR is reserved by zocl driver via device tree. Both PS Linux and PL logic can access PL-DDRs
 *
 * Execution model is asynchronous where execute commands are submitted using command buffers and
 * POSIX poll is used to wait for finished commands. Commands for a compute unit can only be submitted
 * after an explicit context has been opened by the client for that compute unit.
 *
 * *zocl* driver functionality is described in the following table.
 *
 * ==== ====================================== ============================== ==================================
 * #    Functionality                          ioctl request code             data format
 * ==== ====================================== ============================== ==================================
 * 1    Allocate buffer on device              DRM_IOCTL_ZOCL_CREATE_BO       drm_zocl_create_bo
 * 2    Allocate buffer on device with         DRM_IOCTL_ZOCL_USERPTR_BO      drm_zocl_userptr_bo
 *      userptr
 * 3    Get the buffer handle of given         DRM_IOCTL_ZOCL_GET_HOST_BO     drm_zocl_host_bo
 *      physical address
 * 4    Prepare bo for mapping into user's     DRM_IOCTL_ZOCL_MAP_BO          drm_zocl_map_bo
 *      address space
 * 5    Synchronize (DMA) buffer contents in   DRM_IOCTL_ZOCL_SYNC_BO         drm_zocl_sync_bo
 *      requested direction
 * 6    Obtain information about buffer        DRM_IOCTL_ZOCL_INFO_BO         drm_zocl_info_bo
 *      object
 * 7    Update bo backing storage with user's  DRM_IOCTL_ZOCL_PWRITE_BO       drm_zocl_pwrite_bo
 *      data
 * 8    Read back data in bo backing storage   DRM_IOCTL_ZOCL_PREAD_BO        drm_zocl_pread_bo
 * 9    Update device view with a specific     DRM_IOCTL_ZOCL_PCAP_DOWNLOAD   drm_zocl_pcap_download
 *      xclbin image
 * 10   Read the xclbin and map the compute    DRM_IOCTL_ZOCL_READ_AXLF       drm_zocl_axlf
 *      units.
 * 11   Send an execute job to a compute unit  DRM_IOCTL_ZOCL_EXECBUF         drm_zocl_execbuf
 * 12   Get the soft kernel command            DRM_IOCTL_ZOCL_SK_GETCMD       drm_zocl_sk_getcmd
 *      (experimental)
 * 13   Create the soft kernel                 DRM_IOCTL_ZOCL_SK_CREATE       drm_zocl_sk_create
 *      (experimental)
 * 14   Report the soft kernel state           DRM_IOCTL_ZOCL_SK_REPORT       drm_zocl_sk_report
 *      (experimental)
 * 15   Get Information about Compute Unit     DRM_IOCTL_ZOCL_INFO_CU         drm_zocl_info_cu
 *      (experimental)
 *
 * ==== ====================================== ============================== ==================================
 */

#ifndef __ZYNQ_IOCTL_H__
#define __ZYNQ_IOCTL_H__

#ifndef __KERNEL__
#include "drm_mode.h"
#else
#include <uapi/drm/drm_mode.h>
#endif /* !__KERNEL__ */

/*
 * enum drm_zocl_ops - ioctl command code enumerations
 */
enum drm_zocl_ops {
	/* Buffer creation */
	DRM_ZOCL_CREATE_BO = 0,
	/* Buffer creation from user provided pointer */
	DRM_ZOCL_USERPTR_BO,
	/* Get the buffer handle of given physical address */
	DRM_ZOCL_GET_HOST_BO,
	/* Map buffer into application user space (no DMA is performed) */
	DRM_ZOCL_MAP_BO,
	/* Sync buffer (like fsync) in the desired direction by using CPU cache flushing/invalidation */
	DRM_ZOCL_SYNC_BO,
	/* Get information about the buffer such as physical address in the device, etc */
	DRM_ZOCL_INFO_BO,
	/* Update host cached copy of buffer wih user's data */
	DRM_ZOCL_PWRITE_BO,
	/* Update user's data with host cached copy of buffer */
	DRM_ZOCL_PREAD_BO,
	/* Program the Device with specific xclbin image */
	DRM_ZOCL_PCAP_DOWNLOAD,
	/* Send an execute job to a compute unit */
	DRM_ZOCL_EXECBUF,
	/* Read the xclbin and map CUs */
	DRM_ZOCL_READ_AXLF,
	/* Get the soft kernel command */
	DRM_ZOCL_SK_GETCMD,
	/* Create the soft kernel */
	DRM_ZOCL_SK_CREATE,
	/* Report the soft kernel state */
	DRM_ZOCL_SK_REPORT,
	/* Get the information about Compute Unit such as physical address in the device */
	DRM_ZOCL_INFO_CU,
	DRM_ZOCL_NUM_IOCTLS
};

enum drm_zocl_sync_bo_dir {
	DRM_ZOCL_SYNC_BO_TO_DEVICE,
	DRM_ZOCL_SYNC_BO_FROM_DEVICE
};

#define DRM_ZOCL_BO_FLAGS_HOST_BO    (0x1 << 26)
#define DRM_ZOCL_BO_FLAGS_COHERENT   (0x1 << 27)
#define DRM_ZOCL_BO_FLAGS_CMA        (0x1 << 28)
#define DRM_ZOCL_BO_FLAGS_SVM        (0x1 << 29)
#define DRM_ZOCL_BO_FLAGS_USERPTR    (0x1 << 30)
#define DRM_ZOCL_BO_FLAGS_EXECBUF    (0x1 << 31)
/**
 * struct drm_zocl_create_bo - Create buffer object
 * used with DRM_IOCTL_ZOCL_CREATE_BO ioctl
 *
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_ZOCL_BO_XXX flags
 */
struct drm_zocl_create_bo {
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

/**
 * struct drm_zocl_userptr_bo - Create buffer object with user's pointer
 * used with DRM_IOCTL_ZOCL_USERPTR_BO ioctl
 *
 * @addr:       Address of buffer allocated by user
 * @size:       Requested size of the buffer object
 * @handle:     bo handle returned by the driver
 * @flags:      DRM_XOCL_BO_XXX flags
 */
struct drm_zocl_userptr_bo {
	uint64_t addr;
	uint64_t size;
	uint32_t handle;
	uint32_t flags;
};

/**
 * struct drm_zocl_map_bo - Prepare a buffer object for mmap
 * used with DRM_IOCTL_ZOCL_MAP_BO ioctl
 *
 * @handle:     bo handle
 * @pad:        Unused
 * @offset:     'Fake' offset returned by the driver which can be used with POSIX mmap
 */
struct drm_zocl_map_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * struct drm_zocl_sync_bo - Synchronize the buffer in the requested direction
 * via cache flush/invalidation.
 * used with DRM_ZOCL_SYNC_BO ioctl.
 *
 * @handle:	GEM object handle
 * @dir:	DRM_ZOCL_SYNC_DIR_XXX
 * @offset:	Offset into the object to write to
 * @size:	Length of data to write
 */
struct drm_zocl_sync_bo {
	uint32_t handle;
	enum drm_zocl_sync_bo_dir dir;
	uint64_t offset;
	uint64_t size;
};

/**
 * struct drm_zocl_info_bo - Obtain information about buffer object
 * used with DRM_IOCTL_ZOCL_INFO_BO ioctl
 *
 * @handle:	GEM object handle
 * @size:	Size of BO
 * @paddr:	physical address
 */
struct drm_zocl_info_bo {
	uint32_t	handle;
	uint64_t	size;
	uint64_t	paddr;
};

/**
 * struct drm_zocl_host_bo - Get the buffer handle of given physical address
 * used with DRM_IOCTL_ZOCL_GET_HOST_BO ioctl
 *
 * @paddr:	physical address
 * @size:	Size of BO
 * @handle:	GEM object handle
 */
struct drm_zocl_host_bo {
	uint64_t	paddr;
	size_t		size;
	uint32_t	handle;
};

/**
 * struct drm_zocl_pwrite_bo - Update bo with user's data
 * used with DRM_IOCTL_ZOCL_PWRITE_BO ioctl
 *
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the object to write to
 * @size:	Length of data to write
 * @data_ptr:	Pointer to read the data from (pointers not 32/64 compatible)
 */
struct drm_zocl_pwrite_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_zocl_pread_bo - Read data from bo
 * used with DRM_IOCTL_ZOCL_PREAD_BO ioctl
 *
 * @handle:	GEM object handle
 * @pad:	Padding
 * @offset:	Offset into the object to read from
 * @size:	Length of data to wrreadite
 * @data_ptr:	Pointer to write the data into (pointers not 32/64 compatible)
 */
struct drm_zocl_pread_bo {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t data_ptr;
};

/**
 * struct drm_zocl_info_cu - Get information about Compute Unit (experimental)
 * used with DRM_IOCTL_ZOCL_INFO_CU ioctl
 *
 *  @paddr: Physical address
 *  @apt_idx: Aperture index
 */
struct drm_zocl_info_cu {
	uint64_t paddr;
	int apt_idx;
};

/**
 * Opcodes for the embedded scheduler provided by the client to the driver
 */
enum drm_zocl_execbuf_code {
  DRM_ZOCL_EXECBUF_RUN_KERNEL = 0,
  DRM_ZOCL_EXECBUF_RUN_KERNEL_XYZ,
  DRM_ZOCL_EXECBUF_PING,
  DRM_ZOCL_EXECBUF_DEBUG,
};

/**
 * State of exec request managed by the kernel driver
 */
enum drm_zocl_execbuf_state {
  DRM_ZOCL_EXECBUF_STATE_COMPLETE = 0,
  DRM_ZOCL_EXECBUF_STATE_RUNNING,
  DRM_ZOCL_EXECBUF_STATE_SUBMITTED,
  DRM_ZOCL_EXECBUF_STATE_QUEUED,
  DRM_ZOCL_EXECBUF_STATE_ERROR,
  DRM_ZOCL_EXECBUF_STATE_ABORT,
};

/**
 * struct drm_zocl_execbuf - Submit a command buffer for execution on a compute
 * unit  (experimental) used with DRM_IOCTL_ZOCL_EXECBUF ioctl
 *
 * @ctx_id:         Pass 0
 * @exec_bo_handle: BO handle of command buffer formatted as ERT command
 */
struct drm_zocl_execbuf {
  uint32_t ctx_id;
  uint32_t exec_bo_handle;
};

/*
 * BITSTREAM flag is not 1 or 0, in case old shim code might pass in 1 for
 * other flag.  ZOCL kernel will check BITSTRAM and other flags are exclusive.
 */
enum drm_zocl_axlf_flags {
	DRM_ZOCL_AXLF_NONE 		= 0,
	DRM_ZOCL_AXLF_AIE_PDI		= (1 << 0),
	DRM_ZOCL_AXLF_BITSTREAM_PDI	= (1 << 1),
	DRM_ZOCL_AXLF_BITSTREAM		= (1 << 2),
};

/**
 * struct drm_zocl_axlf - Read xclbin (AXLF) device image and map CUs (experimental)
 * used with DRM_IOCTL_ZOCL_READ_AXLF ioctl
 *
 * @axlf  : Pointer to xclbin (AXLF) object
 **/
struct drm_zocl_axlf {
	struct axlf 	*za_xclbin_ptr;
	uint32_t 	za_flags;
};

#define	ZOCL_MAX_NAME_LENGTH		32
#define	ZOCL_MAX_PATH_LENGTH		255

/**
 * struct drm_zocl_sk_getcmd - Get the soft kernel command  (experimental)
 * used with DRM_IOCTL_ZOCL_SK_GETCMD ioctl
 *
 * @opcode       : opcode for the Soft Kernel Command Packet
 * @start_cuidx  : start index of compute units
 * @cu_nums      : number of compute units in program
 * @size         : size in bytes of soft kernel image
 * @paddr        : soft kernel image's physical address (little endian)
 * @name         : symbol name of soft kernel
 */
struct drm_zocl_sk_getcmd {
	uint32_t	opcode;
	uint32_t	start_cuidx;
	uint32_t	cu_nums;
	size_t		size;
	uint64_t	paddr;
	char		name[ZOCL_MAX_NAME_LENGTH];
};

/**
 * struct drm_zocl_sk_create - Create a soft kernel  (experimental)
 * used with DRM_IOCTL_ZOCL_SK_CREATE ioctl
 *
 * @cu_idx     : Compute unit index
 * @handle     : Buffer object handle
 */
struct drm_zocl_sk_create {
	uint32_t	cu_idx;
	uint32_t	handle;
};

/**
 * State of soft compute unit
 */
enum drm_zocl_scu_state {
	ZOCL_SCU_STATE_DONE,
};

/**
 * struct drm_zocl_sk_report- Report the Soft Kernel State  (experimental)
 * used with DRM_IOCTL_ZOCL_SK_REPORT ioctl
 *
 * @cu_idx     : Compute unit index
 * @cu_state   : State of the Soft Compute Unit
 */
struct drm_zocl_sk_report {
	uint32_t		cu_idx;
	enum drm_zocl_scu_state	cu_state;
};

#define DRM_IOCTL_ZOCL_CREATE_BO       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_CREATE_BO,     \
                                       struct drm_zocl_create_bo)
#define DRM_IOCTL_ZOCL_USERPTR_BO      DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_USERPTR_BO,     \
                                       struct drm_zocl_userptr_bo)
#define DRM_IOCTL_ZOCL_GET_HOST_BO     DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_GET_HOST_BO,     \
                                       struct drm_zocl_host_bo)
#define DRM_IOCTL_ZOCL_MAP_BO          DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_MAP_BO, struct drm_zocl_map_bo)
#define DRM_IOCTL_ZOCL_SYNC_BO         DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_SYNC_BO, struct drm_zocl_sync_bo)
#define DRM_IOCTL_ZOCL_INFO_BO         DRM_IOWR(DRM_COMMAND_BASE +          \
                                       DRM_ZOCL_INFO_BO, struct drm_zocl_info_bo)
#define DRM_IOCTL_ZOCL_PWRITE_BO       DRM_IOWR(DRM_COMMAND_BASE +  \
                                       DRM_ZOCL_PWRITE_BO, \
                                       struct drm_zocl_pwrite_bo)
#define DRM_IOCTL_ZOCL_PREAD_BO        DRM_IOWR(DRM_COMMAND_BASE +      \
                                       DRM_ZOCL_PREAD_BO, struct drm_zocl_pread_bo)
#define DRM_IOCTL_ZOCL_PCAP_DOWNLOAD   DRM_IOWR(DRM_COMMAND_BASE +      \
                                       DRM_ZOCL_PCAP_DOWNLOAD, struct drm_zocl_pcap_download)
#define DRM_IOCTL_ZOCL_EXECBUF         DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_EXECBUF, struct drm_zocl_execbuf)
#define DRM_IOCTL_ZOCL_READ_AXLF       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_READ_AXLF, struct drm_zocl_axlf)
#define DRM_IOCTL_ZOCL_SK_GETCMD       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_SK_GETCMD, struct drm_zocl_sk_getcmd)
#define DRM_IOCTL_ZOCL_SK_CREATE       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_SK_CREATE, struct drm_zocl_sk_create)
#define	DRM_IOCTL_ZOCL_SK_REPORT       DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_SK_REPORT, struct drm_zocl_sk_report)
#define DRM_IOCTL_ZOCL_INFO_CU         DRM_IOWR(DRM_COMMAND_BASE + \
                                       DRM_ZOCL_INFO_CU, struct drm_zocl_info_cu)
#endif
