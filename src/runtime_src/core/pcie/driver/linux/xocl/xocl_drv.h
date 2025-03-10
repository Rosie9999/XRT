/*
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
 *          Jan Stephan <j.stephan@hzdr.de>
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

#ifndef	_XOCL_DRV_H_
#define	_XOCL_DRV_H_

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include "xclbin.h"
#include "xrt_mem.h"
#include "devices.h"
#include "xocl_ioctl.h"
#include "mgmt-ioctl.h"
#include "mailbox_proto.h"
#include <linux/libfdt_env.h>
#include "lib/libfdt/libfdt.h"
#include <linux/firmware.h>

/* The fix for the y2k38 bug was introduced with Linux 3.17 and backported to
 * Red Hat 7.2.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	#define XOCL_TIMESPEC struct timespec64
	#define XOCL_GETTIME ktime_get_real_ts64
	#define XOCL_USEC tv_nsec / NSEC_PER_USEC
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2)
		#define XOCL_TIMESPEC struct timespec64
		#define XOCL_GETTIME ktime_get_real_ts64
		#define XOCL_USEC tv_nsec / NSEC_PER_USEC
	#else
		#define XOCL_TIMESPEC struct timeval
		#define XOCL_GETTIME do_gettimeofday
		#define XOCL_USEC tv_usec
	#endif
#else
	#define XOCL_TIMESPEC struct timeval
	#define XOCL_GETTIME do_gettimeofday
	#define XOCL_USEC tv_usec
#endif

/* drm_gem_object_put_unlocked and drm_gem_object_get were introduced with Linux
 * 4.12 and backported to Red Hat 7.5.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
	#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
	#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_get
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5)
		#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
		#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_get
	#else
		#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
		#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_reference
	#endif
#else
	#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
	#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_reference
#endif

/* drm_dev_put was introduced with Linux 4.15 and backported to Red Hat 7.6. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
	#define XOCL_DRM_DEV_PUT drm_dev_put
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
		#define XOCL_DRM_DEV_PUT drm_dev_put
	#else
		#define XOCL_DRM_DEV_PUT drm_dev_unref
	#endif
#else
	#define XOCL_DRM_DEV_PUT drm_dev_unref
#endif

/* access_ok lost its first parameter with Linux 5.0. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	#define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(ADDR, SIZE)
#else
	#define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(TYPE, ADDR, SIZE)
#endif

#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(7, 4)
#define XOCL_UUID
#endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
#define XOCL_UUID
#endif
/* UUID helper functions not present in older kernels */
#if defined(XOCL_UUID)
static inline bool uuid_equal(const xuid_t *u1, const xuid_t *u2)
{
	return memcmp(u1, u2, sizeof(xuid_t)) == 0;
}

static inline void uuid_copy(xuid_t *dst, const xuid_t *src)
{
	memcpy(dst, src, sizeof(xuid_t));
}

static inline bool uuid_is_null(const xuid_t *uuid)
{
	xuid_t uuid_null = NULL_UUID_LE;

	return uuid_equal(uuid, &uuid_null);
}
#endif

static inline void xocl_memcpy_fromio(void *buf, void *iomem, u32 size)
{
	int i;

	BUG_ON(size & 0x3);

	for (i = 0; i < size / 4; i++)
		((u32 *)buf)[i] = ioread32((char *)(iomem) + sizeof(u32) * i);
}

static inline void xocl_memcpy_toio(void *iomem, void *buf, u32 size)
{
	int i;

	BUG_ON(size & 0x3);

	for (i = 0; i < size / 4; i++)
		iowrite32(((u32 *)buf)[i], ((char *)(iomem) + sizeof(u32) * i));
}

#define	XOCL_MODULE_NAME	"xocl"
#define	XCLMGMT_MODULE_NAME	"xclmgmt"
#define	ICAP_XCLBIN_V2		"xclbin2"
#define XOCL_CDEV_DIR		"xfpga"

#define XOCL_MAX_DEVICES	16
#define XOCL_EBUF_LEN           512
#define xocl_sysfs_error(xdev, fmt, args...)     \
		snprintf(((struct xocl_dev_core *)xdev)->ebuf, XOCL_EBUF_LEN,	\
		fmt, ##args)
#define MAX_M_COUNT      	XOCL_SUBDEV_MAX_INST
#define XOCL_MAX_FDT_LEN		1024 * 512

#define	XDEV2DEV(xdev)		(&XDEV(xdev)->pdev->dev)

#define xocl_err(dev, fmt, args...)			\
	dev_err(dev, "%s: "fmt, __func__, ##args)
#define xocl_info(dev, fmt, args...)			\
	dev_info(dev, "%s: "fmt, __func__, ##args)
#define xocl_dbg(dev, fmt, args...)			\
	dev_dbg(dev, "%s: "fmt, __func__, ##args)

#define xocl_xdev_info(xdev, fmt, args...)		\
	xocl_info(XDEV2DEV(xdev), fmt, ##args)
#define xocl_xdev_err(xdev, fmt, args...)		\
	xocl_err(XDEV2DEV(xdev), fmt, ##args)
#define xocl_xdev_dbg(xdev, fmt, args...)		\
	xocl_dbg(XDEV2DEV(xdev), fmt, ##args)

#define	XOCL_DRV_VER_NUM(ma, mi, p)		\
	((ma) * 1000 + (mi) * 100 + (p))

#define	XOCL_READ_REG32(addr)		\
	ioread32(addr)
#define	XOCL_WRITE_REG32(val, addr)	\
	iowrite32(val, addr)

/* xclbin helpers */
#define sizeof_sect(sect, data) \
({ \
	size_t ret; \
	size_t data_size; \
	data_size = (sect) ? sect->m_count * sizeof(typeof(sect->data)) : 0; \
	ret = (sect) ? offsetof(typeof(*sect), data) + data_size : 0; \
	(ret); \
})

#define	XOCL_PL_TO_PCI_DEV(pldev)		\
	to_pci_dev(pldev->dev.parent)

#define XOCL_PL_DEV_TO_XDEV(pldev) \
	pci_get_drvdata(XOCL_PL_TO_PCI_DEV(pldev))

#define XOCL_PCI_DEV_TO_XDEV(pcidev) \
	pci_get_drvdata(pcidev)

#define XOCL_PCI_FUNC(xdev_hdl)		\
	PCI_FUNC(XDEV(xdev_hdl)->pdev->devfn)

#define	XOCL_QDMA_USER_BAR	2
#define	XOCL_DSA_VERSION(xdev)			\
	(XDEV(xdev)->priv.dsa_ver)

#define XOCL_DSA_IS_MPSOC(xdev)                \
	(XDEV(xdev)->priv.mpsoc)

#define XOCL_DSA_IS_SMARTN(xdev)                \
	(XDEV(xdev)->priv.flags & XOCL_DSAFLAG_SMARTN)

#define XOCL_DSA_IS_VERSAL(xdev)                \
	(XDEV(xdev)->priv.flags & XOCL_DSAFLAG_VERSAL)

#define	XOCL_DEV_ID(pdev)			\
	((pci_domain_nr(pdev->bus) << 16) |	\
	PCI_DEVID(pdev->bus->number, pdev->devfn))

#define XOCL_ARE_HOP 0x400000000ull

#define	XOCL_XILINX_VEN		0x10EE
#define	XOCL_CHARDEV_REG_COUNT	16

#ifdef RHEL_RELEASE_VERSION

#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6)
#define RHEL_P2P_SUPPORT_74  0
#define RHEL_P2P_SUPPORT_76  1
#elif RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7, 3) && RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6)
#define RHEL_P2P_SUPPORT_74  1
#define RHEL_P2P_SUPPORT_76  0
#endif
#else
#define RHEL_P2P_SUPPORT_74  0
#define RHEL_P2P_SUPPORT_76  0
#endif


#define RHEL_P2P_SUPPORT (RHEL_P2P_SUPPORT_74 | RHEL_P2P_SUPPORT_76)

#define INVALID_SUBDEVICE ~0U

#define XOCL_INVALID_MINOR -1

extern struct class *xrt_class;

struct drm_xocl_bo;
struct client_ctx;

enum {
	XOCL_SUBDEV_STATE_UNINIT,
	XOCL_SUBDEV_STATE_INIT,
	XOCL_SUBDEV_STATE_ADDED,
	XOCL_SUBDEV_STATE_ATTACHED,
	XOCL_SUBDEV_STATE_OFFLINE,
	XOCL_SUBDEV_STATE_ACTIVE,
};

struct xocl_subdev {
	struct platform_device		*pldev;
	void				*ops;
	int				state;
	struct xocl_subdev_info		info;
	int				inst;
	int				pf;
	struct cdev			*cdev;

        struct resource		res[XOCL_SUBDEV_MAX_RES];
	char	res_name[XOCL_SUBDEV_MAX_RES][XOCL_SUBDEV_RES_NAME_LEN];
	char			bar_idx[XOCL_SUBDEV_MAX_RES];
};

#define XOCL_GET_DRV_PRI(pldev)					\
	(platform_get_device_id(pldev) ?				\
	((struct xocl_drv_private *)				\
	platform_get_device_id(pldev)->driver_data) : NULL)


struct xocl_drv_private {
	void			*ops;
	const struct file_operations	*fops;
	dev_t			dev;
	char			*cdev_name;
};

#define	XOCL_GET_SUBDEV_PRIV(dev)				\
	(dev_get_platdata(dev))

typedef	void *xdev_handle_t;

struct xocl_pci_funcs {
	int (*intr_config)(xdev_handle_t xdev, u32 intr, bool enable);
	int (*intr_register)(xdev_handle_t xdev, u32 intr,
		irq_handler_t handler, void *arg);
	int (*reset)(xdev_handle_t xdev);
};

#define	XDEV(dev)	((struct xocl_dev_core *)(dev))
#define	XDEV_PCIOPS(xdev)	(XDEV(xdev)->pci_ops)

#define	xocl_user_interrupt_config(xdev, intr, en)	\
	XDEV_PCIOPS(xdev)->intr_config(xdev, intr, en)
#define	xocl_user_interrupt_reg(xdev, intr, handler, arg)	\
	XDEV_PCIOPS(xdev)->intr_register(xdev, intr, handler, arg)
#define xocl_reset(xdev)			\
	(XDEV_PCIOPS(xdev)->reset ? XDEV_PCIOPS(xdev)->reset(xdev) : \
	-ENODEV)

struct xocl_thread_arg {
	int (*thread_cb)(void *arg);
	void		*arg;
	u32		interval;    /* ms */
	struct device	*dev;
	char		*name;
};

struct xocl_drvinst_proc {
	struct list_head	link;
	u32			pid;
	u32			count;
};

/*
 * Base structure for platform driver data structures
 */
struct xocl_drvinst {
	struct device		*dev;
	u32			size;
	atomic_t		ref;
	struct completion	comp;
	struct list_head	open_procs;
	void			*file_dev;
	bool			offline;
        /*
	 * The derived object placed inline in field "data"
	 * should be aligned at 8 byte boundary
	 */
        u64			data[1];
};

struct xocl_dev_core {
	struct pci_dev		*pdev;
	int			dev_minor;
	struct xocl_subdev	*subdevs[XOCL_SUBDEV_NUM];
	struct xocl_subdev	*dyn_subdev_store;
	int			dyn_subdev_num;
	struct xocl_pci_funcs	*pci_ops;

	struct mutex 		lock;

	u32			bar_idx;
	void __iomem		*bar_addr;
	resource_size_t		bar_size;
	resource_size_t		feature_rom_offset;

	u32			intr_bar_idx;
	void __iomem		*intr_bar_addr;
	resource_size_t		intr_bar_size;

	struct task_struct      *poll_thread;
	struct xocl_thread_arg thread_arg;

	struct xocl_drm		*drm;

	char			*fdt_blob;
	struct xocl_board_private priv;

	rwlock_t		rwlock;

	char			ebuf[XOCL_EBUF_LEN + 1];
};

#define XOCL_DRM(xdev_hdl)					\
	(((struct xocl_dev_core *)xdev_hdl)->drm)

#define	XOCL_DSA_PCI_RESET_OFF(xdev_hdl)			\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_PCI_RESET_OFF)
#define	XOCL_DSA_MB_SCHE_OFF(xdev_hdl)				\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_MB_SCHE_OFF)
#define	XOCL_DSA_AXILITE_FLUSH_REQUIRED(xdev_hdl)		\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_AXILITE_FLUSH)
#define	XOCL_DSA_NO_KDMA(xdev_hdl)				\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_NO_KDMA)

#define	XOCL_DSA_XPR_ON(xdev_hdl)		\
	(((struct xocl_dev_core *)xdev_hdl)->priv.xpr)


#define	SUBDEV(xdev, id)	\
	(XDEV(xdev)->subdevs[id][0])
#define	SUBDEV_MULTI(xdev, id, idx)	\
	(XDEV(xdev)->subdevs[id][idx])

struct xocl_subdev_funcs {
	int (*offline)(struct platform_device *pdev);
	int (*online)(struct platform_device *pdev);
};

#define offline_cb common_funcs.offline
#define online_cb common_funcs.online

/* rom callbacks */
struct xocl_rom_funcs {
	struct xocl_subdev_funcs common_funcs;
	bool (*is_unified)(struct platform_device *pdev);
	bool (*mb_mgmt_on)(struct platform_device *pdev);
	bool (*mb_sched_on)(struct platform_device *pdev);
	uint32_t* (*cdma_addr)(struct platform_device *pdev);
	u16 (*get_ddr_channel_count)(struct platform_device *pdev);
	u64 (*get_ddr_channel_size)(struct platform_device *pdev);
	bool (*is_are)(struct platform_device *pdev);
	bool (*is_aws)(struct platform_device *pdev);
	bool (*verify_timestamp)(struct platform_device *pdev, u64 timestamp);
	u64 (*get_timestamp)(struct platform_device *pdev);
	int (*get_raw_header)(struct platform_device *pdev, void *header);
	bool (*runtime_clk_scale_on)(struct platform_device *pdev);
	int (*find_firmware)(struct platform_device *pdev, char *fw_name,
		size_t len, u16 deviceid, const struct firmware **fw);
	bool (*passthrough_virtualization_on)(struct platform_device *pdev);
};

#define ROM_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).pldev
#define	ROM_OPS(xdev)	\
	((struct xocl_rom_funcs *)SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).ops)
#define ROM_CB(xdev, cb)	\
	(ROM_DEV(xdev) && ROM_OPS(xdev) && ROM_OPS(xdev)->cb)
#define	xocl_is_unified(xdev)		\
	(ROM_CB(xdev, is_unified) ? ROM_OPS(xdev)->is_unified(ROM_DEV(xdev)) : true)
#define	xocl_mb_mgmt_on(xdev)		\
	(ROM_CB(xdev, mb_mgmt_on) ? ROM_OPS(xdev)->mb_mgmt_on(ROM_DEV(xdev)) : false)
#define	xocl_mb_sched_on(xdev)		\
	(ROM_CB(xdev, mb_sched_on) ? ROM_OPS(xdev)->mb_sched_on(ROM_DEV(xdev)) : false)
#define	xocl_rom_cdma_addr(xdev)		\
	(ROM_CB(xdev, cdma_addr) ? ROM_OPS(xdev)->cdma_addr(ROM_DEV(xdev)) : 0)
#define xocl_clk_scale_on(xdev)		\
	(ROM_CB(xdev, runtime_clk_scale_on) ? ROM_OPS(xdev)->runtime_clk_scale_on(ROM_DEV(xdev)) : false)
#define	xocl_get_ddr_channel_count(xdev) \
	(ROM_CB(xdev, get_ddr_channel_count) ? ROM_OPS(xdev)->get_ddr_channel_count(ROM_DEV(xdev)) :\
	0)
#define	xocl_get_ddr_channel_size(xdev) \
	(ROM_CB(xdev, get_ddr_channel_size) ? ROM_OPS(xdev)->get_ddr_channel_size(ROM_DEV(xdev)) : 0)
#define	xocl_is_are(xdev)		\
	(ROM_CB(xdev, is_are) ? ROM_OPS(xdev)->is_are(ROM_DEV(xdev)) : false)
#define	xocl_is_aws(xdev)		\
	(ROM_CB(xdev, is_aws) ? ROM_OPS(xdev)->is_aws(ROM_DEV(xdev)) : false)
#define	xocl_verify_timestamp(xdev, ts)	\
	(ROM_CB(xdev, verify_timestamp) ? ROM_OPS(xdev)->verify_timestamp(ROM_DEV(xdev), ts) : \
	false)
#define	xocl_get_timestamp(xdev) \
	(ROM_CB(xdev, get_timestamp) ? ROM_OPS(xdev)->get_timestamp(ROM_DEV(xdev)) : 0)
#define	xocl_get_raw_header(xdev, header) \
	(ROM_CB(xdev, get_raw_header) ? ROM_OPS(xdev)->get_raw_header(ROM_DEV(xdev), header) :\
	-ENODEV)
#define xocl_rom_find_firmware(xdev, fw_name, len, deviceid, fw)	\
	(ROM_CB(xdev, find_firmware) ? ROM_OPS(xdev)->find_firmware(	\
	ROM_DEV(xdev), fw_name, len, deviceid, fw) : -ENODEV)
#define xocl_passthrough_virtualization_on(xdev)		\
	(ROM_CB(xdev, passthrough_virtualization_on) ?		\
	ROM_OPS(xdev)->passthrough_virtualization_on(ROM_DEV(xdev)) : false)

/* dma callbacks */
struct xocl_dma_funcs {
	struct xocl_subdev_funcs common_funcs;
	ssize_t (*migrate_bo)(struct platform_device *pdev,
		struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 sz);
	int (*ac_chan)(struct platform_device *pdev, u32 dir);
	void (*rel_chan)(struct platform_device *pdev, u32 dir, u32 channel);
	u32 (*get_chan_count)(struct platform_device *pdev);
	u64 (*get_chan_stat)(struct platform_device *pdev, u32 channel,
		u32 write);
	u64 (*get_str_stat)(struct platform_device *pdev, u32 q_idx);
	int (*user_intr_config)(struct platform_device *pdev, u32 intr, bool en);
	int (*user_intr_register)(struct platform_device *pdev, u32 intr,
					irq_handler_t handler, void *arg, int event_fd);
	int (*user_intr_unreg)(struct platform_device *pdev, u32 intr);
};

#define DMA_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_DMA).pldev
#define	DMA_OPS(xdev)	\
	((struct xocl_dma_funcs *)SUBDEV(xdev, XOCL_SUBDEV_DMA).ops)
#define DMA_CB(xdev, cb)	\
	(DMA_DEV(xdev) && DMA_OPS(xdev) && DMA_OPS(xdev)->cb)
#define	xocl_migrate_bo(xdev, sgt, to_dev, paddr, chan, len)	\
	(DMA_CB(xdev, migrate_bo) ? DMA_OPS(xdev)->migrate_bo(DMA_DEV(xdev), \
	sgt, to_dev, paddr, chan, len) : 0)
#define	xocl_acquire_channel(xdev, dir)		\
	(DMA_CB(xdev, ac_chan) ? DMA_OPS(xdev)->ac_chan(DMA_DEV(xdev), dir) : \
	-ENODEV)
#define	xocl_release_channel(xdev, dir, chan)	\
	(DMA_CB(xdev, rel_chan) ? DMA_OPS(xdev)->rel_chan(DMA_DEV(xdev), dir, \
	chan) : NULL)
#define	xocl_get_chan_count(xdev)		\
	(DMA_CB(xdev, get_chan_count) ? DMA_OPS(xdev)->get_chan_count(DMA_DEV(xdev)) \
	: 0)
#define	xocl_get_chan_stat(xdev, chan, write)		\
	(DMA_CB(xdev, get_chan_stat) ? DMA_OPS(xdev)->get_chan_stat(DMA_DEV(xdev), \
	chan, write) : 0)
#define xocl_dma_intr_config(xdev, irq, en)			\
	(DMA_CB(xdev, user_intr_config) ? DMA_OPS(xdev)->user_intr_config(DMA_DEV(xdev), \
	irq, en) : -ENODEV)
#define xocl_dma_intr_register(xdev, irq, handler, arg, event_fd)	\
	(DMA_CB(xdev, user_intr_register) ? DMA_OPS(xdev)->user_intr_register(DMA_DEV(xdev), \
	irq, handler, arg, event_fd) : -ENODEV)
#define xocl_dma_intr_unreg(xdev, irq)				\
	(DMA_CB(xdev, user_intr_unreg) ? DMA_OPS(xdev)->user_intr_unreg(DMA_DEV(xdev),	\
	irq) : -ENODEV)

/* mb_scheduler callbacks */
struct xocl_mb_scheduler_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*create_client)(struct platform_device *pdev, void **priv);
	void (*destroy_client)(struct platform_device *pdev, void **priv);
	uint (*poll_client)(struct platform_device *pdev, struct file *filp,
		poll_table *wait, void *priv);
	int (*client_ioctl)(struct platform_device *pdev, int op,
		void *data, void *drm_filp);
	int (*stop)(struct platform_device *pdev);
	int (*reset)(struct platform_device *pdev, const xuid_t *xclbin_id);
	int (*reconfig)(struct platform_device *pdev);
	int (*cu_map_addr)(struct platform_device *pdev, u32 cu_index,
		void *drm_filp, u32 *addrp);
};
#define	MB_SCHEDULER_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MB_SCHEDULER).pldev
#define	MB_SCHEDULER_OPS(xdev)	\
	((struct xocl_mb_scheduler_funcs *)SUBDEV(xdev,		\
		XOCL_SUBDEV_MB_SCHEDULER).ops)
#define SCHE_CB(xdev, cb)	\
	(MB_SCHEDULER_DEV(xdev) && MB_SCHEDULER_OPS(xdev))
#define	xocl_exec_create_client(xdev, priv)		\
	(SCHE_CB(xdev, create_client) ?			\
	MB_SCHEDULER_OPS(xdev)->create_client(MB_SCHEDULER_DEV(xdev), priv) : \
	-ENODEV)
#define	xocl_exec_destroy_client(xdev, priv)		\
	(SCHE_CB(xdev, destroy_client) ?			\
	MB_SCHEDULER_OPS(xdev)->destroy_client(MB_SCHEDULER_DEV(xdev), priv) : \
	NULL)
#define	xocl_exec_poll_client(xdev, filp, wait, priv)		\
	(SCHE_CB(xdev, poll_client) ?				\
	MB_SCHEDULER_OPS(xdev)->poll_client(MB_SCHEDULER_DEV(xdev), filp, \
	wait, priv) : 0)
#define	xocl_exec_client_ioctl(xdev, op, data, drm_filp)		\
	(SCHE_CB(xdev, client_ioctl) ?				\
	MB_SCHEDULER_OPS(xdev)->client_ioctl(MB_SCHEDULER_DEV(xdev),	\
	op, data, drm_filp) : -ENODEV)
#define	xocl_exec_stop(xdev)		\
	(SCHE_CB(xdev, stop) ?				\
	 MB_SCHEDULER_OPS(xdev)->stop(MB_SCHEDULER_DEV(xdev)) : \
	-ENODEV)
#define	xocl_exec_reset(xdev, xclbin_id)		\
	(SCHE_CB(xdev, reset) ?				\
	 MB_SCHEDULER_OPS(xdev)->reset(MB_SCHEDULER_DEV(xdev), xclbin_id) : \
	-ENODEV)
#define	xocl_exec_reconfig(xdev)		\
	(SCHE_CB(xdev, reconfig) ?				\
	 MB_SCHEDULER_OPS(xdev)->reconfig(MB_SCHEDULER_DEV(xdev)) : \
	-ENODEV)
#define	xocl_exec_cu_map_addr(xdev, cu, filep, addrp)		\
	(SCHE_CB(xdev, cu_map_addr) ?				\
	MB_SCHEDULER_OPS(xdev)->cu_map_addr(			\
	MB_SCHEDULER_DEV(xdev), cu, filep, addrp) :		\
	-ENODEV)

#define XOCL_MEM_TOPOLOGY(xdev)						\
	((struct mem_topology *)xocl_icap_get_data(xdev, MEMTOPO_AXLF))
#define XOCL_IP_LAYOUT(xdev)						\
	((struct ip_layout *)xocl_icap_get_data(xdev, IPLAYOUT_AXLF))
#define XOCL_XCLBIN_ID(xdev)						\
	((xuid_t *)xocl_icap_get_data(xdev, XCLBIN_UUID))

#define	XOCL_IS_DDR_USED(xdev, ddr)					\
	(XOCL_MEM_TOPOLOGY(xdev)->m_mem_data[ddr].m_used == 1)
#define	XOCL_DDR_COUNT_UNIFIED(xdev)		\
	(XOCL_MEM_TOPOLOGY(xdev) ? XOCL_MEM_TOPOLOGY(xdev)->m_count : 0)
#define	XOCL_DDR_COUNT(xdev)			\
	((xocl_is_unified(xdev) ? XOCL_DDR_COUNT_UNIFIED(xdev) :	\
	xocl_get_ddr_channel_count(xdev)))
#define XOCL_IS_STREAM(topo, idx)					\
	(topo->m_mem_data[idx].m_type == MEM_STREAMING || \
	 topo->m_mem_data[idx].m_type == MEM_STREAMING_CONNECTION)

/* sysmon callbacks */
enum {
	XOCL_SYSMON_PROP_TEMP,
	XOCL_SYSMON_PROP_TEMP_MAX,
	XOCL_SYSMON_PROP_TEMP_MIN,
	XOCL_SYSMON_PROP_VCC_INT,
	XOCL_SYSMON_PROP_VCC_INT_MAX,
	XOCL_SYSMON_PROP_VCC_INT_MIN,
	XOCL_SYSMON_PROP_VCC_AUX,
	XOCL_SYSMON_PROP_VCC_AUX_MAX,
	XOCL_SYSMON_PROP_VCC_AUX_MIN,
	XOCL_SYSMON_PROP_VCC_BRAM,
	XOCL_SYSMON_PROP_VCC_BRAM_MAX,
	XOCL_SYSMON_PROP_VCC_BRAM_MIN,
};
struct xocl_sysmon_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
};
#define	SYSMON_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_SYSMON).pldev
#define	SYSMON_OPS(xdev)	\
	((struct xocl_sysmon_funcs *)SUBDEV(xdev,	\
		XOCL_SUBDEV_SYSMON).ops)
#define SYSMON_CB(xdev, cb)	\
	(SYSMON_DEV(xdev) && SYSMON_OPS(xdev) && SYSMON_OPS(xdev)->cb)
#define	xocl_sysmon_get_prop(xdev, prop, val)		\
	(SYSMON_CB(xdev, get_prop) ? SYSMON_OPS(xdev)->get_prop(SYSMON_DEV(xdev), \
	prop, val) : -ENODEV)

/* firewall callbacks */
enum {
	XOCL_AF_PROP_TOTAL_LEVEL,
	XOCL_AF_PROP_STATUS,
	XOCL_AF_PROP_LEVEL,
	XOCL_AF_PROP_DETECTED_STATUS,
	XOCL_AF_PROP_DETECTED_LEVEL,
	XOCL_AF_PROP_DETECTED_TIME,
};
struct xocl_firewall_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
	int (*clear_firewall)(struct platform_device *pdev);
	u32 (*check_firewall)(struct platform_device *pdev, int *level);
	void (*get_data)(struct platform_device *pdev, void *buf);
};
#define AF_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_AF).pldev
#define	AF_OPS(xdev)	\
	((struct xocl_firewall_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_AF).ops)
#define AF_CB(xdev, cb)	\
	(AF_DEV(xdev) && AF_OPS(xdev) && AF_OPS(xdev)->cb)
#define	xocl_af_get_prop(xdev, prop, val)		\
	(AF_CB(xdev, get_prop) ? AF_OPS(xdev)->get_prop(AF_DEV(xdev), prop, val) : \
	-ENODEV)
#define	xocl_af_check(xdev, level)			\
	(AF_CB(xdev, check_firewall) ? AF_OPS(xdev)->check_firewall(AF_DEV(xdev), level) : 0)
#define	xocl_af_clear(xdev)				\
	(AF_CB(xdev, clear_firewall) ? AF_OPS(xdev)->clear_firewall(AF_DEV(xdev)) : -ENODEV)
#define	xocl_af_get_data(xdev, buf)				\
	(AF_CB(xdev, get_data) ? AF_OPS(xdev)->get_data(AF_DEV(xdev), buf) : -ENODEV)

/* microblaze callbacks */
struct xocl_mb_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*reset)(struct platform_device *pdev);
	int (*stop)(struct platform_device *pdev);
	int (*load_mgmt_image)(struct platform_device *pdev, const char *buf,
		u32 len);
	int (*load_sche_image)(struct platform_device *pdev, const char *buf,
		u32 len);
	int (*get_data)(struct platform_device *pdev, enum group_kind kind, void *buf);
	int (*dr_freeze)(struct platform_device *pdev);
	int (*dr_free)(struct platform_device *pdev);
};

#define	MB_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_MB).pldev
#define	MB_OPS(xdev)		\
	((struct xocl_mb_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_MB).ops)
#define MB_CB(xdev, cb)	\
	(MB_DEV(xdev) && MB_OPS(xdev) && MB_OPS(xdev)->cb)
#define	xocl_mb_reset(xdev)			\
	(MB_CB(xdev, reset) ? MB_OPS(xdev)->reset(MB_DEV(xdev)) : NULL) \

#define	xocl_mb_stop(xdev)			\
	(MB_CB(xdev, stop) ? MB_OPS(xdev)->stop(MB_DEV(xdev)) : -ENODEV)

#define xocl_mb_load_mgmt_image(xdev, buf, len)		\
	(MB_CB(xdev, load_mgmt_image) ? MB_OPS(xdev)->load_mgmt_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)
#define xocl_mb_load_sche_image(xdev, buf, len)		\
	(MB_CB(xdev, load_sche_image) ? MB_OPS(xdev)->load_sche_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)

#define xocl_xmc_get_data(xdev, kind, buf)			\
	(MB_CB(xdev, get_data) ? MB_OPS(xdev)->get_data(MB_DEV(xdev), kind, buf) : -ENODEV)

#define xocl_xmc_dr_freeze(xdev)		\
	(MB_CB(xdev, dr_freeze) ? MB_OPS(xdev)->dr_freeze(MB_DEV(xdev)) : -ENODEV)
#define xocl_xmc_dr_free(xdev)		\
	(MB_CB(xdev, dr_free) ? MB_OPS(xdev)->dr_free(MB_DEV(xdev)) : -ENODEV)

struct xocl_dna_funcs {
	struct xocl_subdev_funcs common_funcs;
	u32 (*status)(struct platform_device *pdev);
	u32 (*capability)(struct platform_device *pdev);
	void (*write_cert)(struct platform_device *pdev, const uint32_t *buf, u32 len);
	void (*get_data)(struct platform_device *pdev, void *buf);
};

#define	DNA_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_DNA).pldev
#define	DNA_OPS(xdev)		\
	((struct xocl_dna_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_DNA).ops)
#define DNA_CB(xdev, cb)	\
	(DNA_DEV(xdev) && DNA_OPS(xdev) && DNA_OPS(xdev)->cb)
#define	xocl_dna_status(xdev)			\
	(DNA_CB(xdev, status) ? DNA_OPS(xdev)->status(DNA_DEV(xdev)) : 0)
#define	xocl_dna_capability(xdev)			\
	(DNA_CB(xdev, capability) ? DNA_OPS(xdev)->capability(DNA_DEV(xdev)) : 2)
#define xocl_dna_write_cert(xdev, data, len)  \
	(DNA_CB(xdev, write_cert) ? DNA_OPS(xdev)->write_cert(DNA_DEV(xdev), data, len) : 0)
#define xocl_dna_get_data(xdev, buf)  \
	(DNA_CB(xdev, get_data) ? DNA_OPS(xdev)->get_data(DNA_DEV(xdev), buf) : 0)
/**
 *	data_kind
 */

enum data_kind {
	MIG_CALIB,
	DIMM0_TEMP,
	DIMM1_TEMP,
	DIMM2_TEMP,
	DIMM3_TEMP,
	FPGA_TEMP,
	CLOCK_FREQ_0,
	CLOCK_FREQ_1,
	FREQ_COUNTER_0,
	FREQ_COUNTER_1,
	VOL_12V_PEX,
	VOL_12V_AUX,
	CUR_12V_PEX,
	CUR_12V_AUX,
	SE98_TEMP0,
	SE98_TEMP1,
	SE98_TEMP2,
	FAN_TEMP,
	FAN_RPM,
	VOL_3V3_PEX,
	VOL_3V3_AUX,
	VPP_BTM,
	VPP_TOP,
	VOL_5V5_SYS,
	VOL_1V2_TOP,
	VOL_1V2_BTM,
	VOL_1V8,
	VCC_0V9A,
	VOL_12V_SW,
	VTT_MGTA,
	VOL_VCC_INT,
	CUR_VCC_INT,
	IDCODE,
	IPLAYOUT_AXLF,
	MEMTOPO_AXLF,
	CONNECTIVITY_AXLF,
	DEBUG_IPLAYOUT_AXLF,
	PEER_CONN,
	XCLBIN_UUID,
	CLOCK_FREQ_2,
	CLOCK_FREQ_3,
	FREQ_COUNTER_2,
	FREQ_COUNTER_3,
	PEER_UUID,
	HBM_TEMP,
	CAGE_TEMP0,
	CAGE_TEMP1,
	CAGE_TEMP2,
	CAGE_TEMP3,
	VCC_0V85,
	SER_NUM,
	MAC_ADDR0,
	MAC_ADDR1,
	MAC_ADDR2,
	MAC_ADDR3,
	REVISION,
	CARD_NAME,
	BMC_VER,
	MAX_PWR,
	FAN_PRESENCE,
	CFG_MODE,
	VOL_VCC_3V3,
	CUR_3V3_PEX,
	CUR_VCC_0V85,
	VOL_HBM_1V2,
	VOL_VPP_2V5,
	VOL_VCCINT_BRAM,
};

enum mb_kind {
	CHAN_STATE,
	CHAN_SWITCH,
	COMM_ID,
	VERSION,
};

typedef	void (*mailbox_msg_cb_t)(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch);
struct xocl_mailbox_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*request)(struct platform_device *pdev, void *req,
		size_t reqlen, void *resp, size_t *resplen,
		mailbox_msg_cb_t cb, void *cbarg, u32 timeout);
	int (*post_notify)(struct platform_device *pdev, void *req, size_t len);
	int (*post_response)(struct platform_device *pdev,
		enum mailbox_request req, u64 reqid, void *resp, size_t len);
	int (*listen)(struct platform_device *pdev,
		mailbox_msg_cb_t cb, void *cbarg);
	int (*set)(struct platform_device *pdev, enum mb_kind kind, u64 data);
	int (*get)(struct platform_device *pdev, enum mb_kind kind, u64 *data);
};
#define	MAILBOX_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).pldev
#define	MAILBOX_OPS(xdev)	\
	((struct xocl_mailbox_funcs *)SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).ops)
#define MAILBOX_READY(xdev, cb)	\
	(MAILBOX_DEV(xdev) && MAILBOX_OPS(xdev) && MAILBOX_OPS(xdev)->cb)
#define	xocl_peer_request(xdev, req, reqlen, resp, resplen, cb, cbarg, timeout)	\
	(MAILBOX_READY(xdev, request) ? MAILBOX_OPS(xdev)->request(MAILBOX_DEV(xdev), \
	req, reqlen, resp, resplen, cb, cbarg, timeout) : -ENODEV)
#define	xocl_peer_response(xdev, req, reqid, buf, len)			\
	(MAILBOX_READY(xdev, post_response) ? MAILBOX_OPS(xdev)->post_response(	\
	MAILBOX_DEV(xdev), req, reqid, buf, len) : -ENODEV)
#define	xocl_peer_notify(xdev, req, reqlen)				\
	(MAILBOX_READY(xdev, post_notify) ? MAILBOX_OPS(xdev)->post_notify(		\
	MAILBOX_DEV(xdev), req, reqlen) : -ENODEV)
#define	xocl_peer_listen(xdev, cb, cbarg)				\
	(MAILBOX_READY(xdev, listen) ? MAILBOX_OPS(xdev)->listen(MAILBOX_DEV(xdev), \
	cb, cbarg) : -ENODEV)
#define	xocl_mailbox_set(xdev, kind, data)				\
	(MAILBOX_READY(xdev, set) ? MAILBOX_OPS(xdev)->set(MAILBOX_DEV(xdev), \
	kind, data) : -ENODEV)
#define	xocl_mailbox_get(xdev, kind, data)				\
	(MAILBOX_READY(xdev, get) ? MAILBOX_OPS(xdev)->get(MAILBOX_DEV(xdev), \
	kind, data) : -ENODEV)

struct xocl_icap_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*reset_axi_gate)(struct platform_device *pdev);
	int (*reset_bitstream)(struct platform_device *pdev);
	int (*download_bitstream_axlf)(struct platform_device *pdev,
		const void __user *arg);
	int (*download_boot_firmware)(struct platform_device *pdev);
	int (*download_rp)(struct platform_device *pdev, int level, int flag);
	int (*post_download_rp)(struct platform_device *pdev);
	int (*ocl_set_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	int (*ocl_get_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	int (*ocl_update_clock_freq_topology)(struct platform_device *pdev, struct xclmgmt_ioc_freqscaling *freqs);
	int (*ocl_lock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid);
	int (*ocl_unlock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid);
	uint64_t (*get_data)(struct platform_device *pdev,
		enum data_kind kind);
};
enum {
	RP_DOWNLOAD_NORMAL,
	RP_DOWNLOAD_DRY,
	RP_DOWNLOAD_FORCE,
	RP_DOWNLOAD_CLEAR,
};
#define	ICAP_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_ICAP).pldev
#define	ICAP_OPS(xdev)							\
	((struct xocl_icap_funcs *)SUBDEV(xdev, XOCL_SUBDEV_ICAP).ops)
#define ICAP_CB(xdev, cb)						\
	(ICAP_DEV(xdev) && ICAP_OPS(xdev) && ICAP_OPS(xdev)->cb)
#define	xocl_icap_reset_axi_gate(xdev)					\
	(ICAP_CB(xdev, reset_axi_gate) ?				\
	ICAP_OPS(xdev)->reset_axi_gate(ICAP_DEV(xdev)) :		\
	NULL)
#define	xocl_icap_reset_bitstream(xdev)					\
	(ICAP_CB(xdev, reset_bitstream) ?				\
	ICAP_OPS(xdev)->reset_bitstream(ICAP_DEV(xdev)) :		\
	-ENODEV)
#define	xocl_icap_download_axlf(xdev, xclbin)				\
	(ICAP_CB(xdev, download_bitstream_axlf) ?			\
	ICAP_OPS(xdev)->download_bitstream_axlf(ICAP_DEV(xdev), xclbin) : \
	-ENODEV)
#define	xocl_icap_download_boot_firmware(xdev)				\
	(ICAP_CB(xdev, download_boot_firmware) ?			\
	ICAP_OPS(xdev)->download_boot_firmware(ICAP_DEV(xdev)) :	\
	-ENODEV)
#define xocl_icap_download_rp(xdev, level, flag)			\
	(ICAP_CB(xdev, download_rp) ?					\
	ICAP_OPS(xdev)->download_rp(ICAP_DEV(xdev), level, flag) :	\
	-ENODEV)
#define xocl_icap_post_download_rp(xdev)				\
	(ICAP_CB(xdev, post_download_rp) ?				\
	ICAP_OPS(xdev)->post_download_rp(ICAP_DEV(xdev)) :		\
	-ENODEV)
#define	xocl_icap_ocl_get_freq(xdev, region, freqs, num)		\
	(ICAP_CB(xdev, ocl_get_freq) ?					\
	ICAP_OPS(xdev)->ocl_get_freq(ICAP_DEV(xdev), region, freqs, num) : \
	-ENODEV)
#define	xocl_icap_ocl_update_clock_freq_topology(xdev, freqs)		\
	(ICAP_CB(xdev, ocl_update_clock_freq_topology) ?		\
	ICAP_OPS(xdev)->ocl_update_clock_freq_topology(ICAP_DEV(xdev), freqs) :\
	-ENODEV)
#define	xocl_icap_ocl_set_freq(xdev, region, freqs, num)		\
	(ICAP_CB(xdev, ocl_set_freq) ?					\
	ICAP_OPS(xdev)->ocl_set_freq(ICAP_DEV(xdev), region, freqs, num) : \
	-ENODEV)
#define	xocl_icap_lock_bitstream(xdev, uuid)				\
	(ICAP_CB(xdev, ocl_lock_bitstream) ?				\
	ICAP_OPS(xdev)->ocl_lock_bitstream(ICAP_DEV(xdev), uuid) :	\
	-ENODEV)
#define	xocl_icap_unlock_bitstream(xdev, uuid)				\
	(ICAP_CB(xdev, ocl_unlock_bitstream) ?				\
	ICAP_OPS(xdev)->ocl_unlock_bitstream(ICAP_DEV(xdev), uuid) :	\
	-ENODEV)
#define xocl_icap_refresh_addrs(xdev)					\
	(ICAP_CB(xdev, refresh_addrs) ?					\
	ICAP_OPS(xdev)->refresh_addrs(ICAP_DEV(xdev)) : NULL)
#define	xocl_icap_get_data(xdev, kind)					\
	(ICAP_CB(xdev, get_data) ?					\
	ICAP_OPS(xdev)->get_data(ICAP_DEV(xdev), kind) : 		\
	0)

struct xocl_mig_label {
	unsigned char	tag[16];
	uint64_t	mem_idx;
	enum MEM_TYPE	mem_type;	
};

struct xocl_mig_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*get_data)(struct platform_device *pdev, void *buf, size_t entry_sz);
	void (*set_data)(struct platform_device *pdev, void *buf);
	uint32_t (*get_id)(struct platform_device *pdev);
};

#define	MIG_DEV(xdev, idx)	SUBDEV_MULTI(xdev, XOCL_SUBDEV_MIG, idx).pldev
#define	MIG_OPS(xdev, idx)							\
	((struct xocl_mig_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_MIG, idx).ops)
#define	MIG_CB(xdev, idx)	\
	(MIG_DEV(xdev, idx) && MIG_OPS(xdev, idx))
#define	xocl_mig_get_data(xdev, idx, buf, entry_sz)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->get_data(MIG_DEV(xdev, idx), buf, entry_sz) : \
	0)
#define	xocl_mig_set_data(xdev, idx, buf)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->set_data(MIG_DEV(xdev, idx), buf) : \
	0)
#define	xocl_mig_get_id(xdev, idx)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->get_id(MIG_DEV(xdev, idx)) : \
	0)


struct xocl_iores_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*read32)(struct platform_device *pdev, u32 id, u32 off, u32 *val);
	int (*write32)(struct platform_device *pdev, u32 id, u32 off, u32 val);
	void __iomem *(*get_base)(struct platform_device *pdev, u32 id);
	uint64_t (*get_offset)(struct platform_device *pdev, u32 id);
};

#define IORES_DEV(xdev, idx)  SUBDEV_MULTI(xdev, XOCL_SUBDEV_IORES, idx).pldev
#define	IORES_OPS(xdev, idx)						\
	((struct xocl_iores_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_IORES, idx).ops)
#define IORES_CB(xdev, idx, cb)		\
	(IORES_DEV(xdev, idx) && IORES_OPS(xdev, idx) &&		\
	IORES_OPS(xdev, idx)->cb)
#define	xocl_iores_read32(xdev, level, id, off, val)			\
	(IORES_CB(xdev, level, read32) ?				\
	IORES_OPS(xdev, level)->read32(IORES_DEV(xdev, level), id, off, val) :\
	-ENODEV)
#define	xocl_iores_write32(xdev, level, id, off, val)			\
	(IORES_CB(xdev, level, write32) ?				\
	IORES_OPS(xdev, level)->write32(IORES_DEV(xdev, level), id, off, val) :\
	-ENODEV)
#define __get_base(xdev, level, id)				\
	(IORES_CB(xdev, level, get_base) ?				\
	IORES_OPS(xdev, level)->get_base(IORES_DEV(xdev, level), id) : NULL)
static inline void __iomem *xocl_iores_get_base(xdev_handle_t xdev, int id)
{
	void __iomem *base;
	int i;

	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i >= 0; i--) {
		base = __get_base(xdev, i, id);
		if (base)
			return base;
	}

	return NULL;
}
#define __get_offset(xdev, level, id)				\
	(IORES_CB(xdev, level, get_offset) ?				\
	IORES_OPS(xdev, level)->get_offset(IORES_DEV(xdev, level), id) : -1)
static inline uint64_t xocl_iores_get_offset(xdev_handle_t xdev, int id)
{
	uint64_t offset;
	int i;

	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i >= 0; i--) {
		offset = __get_offset(xdev, i, id);
		if (offset != (uint64_t)-1)
			return offset;
	}

	return -1;
}


struct xocl_axigate_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*freeze)(struct platform_device *pdev);
	int (*free)(struct platform_device *pdev);
};

#define AXIGATE_DEV(xdev, idx)			\
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_AXIGATE, idx).pldev
#define AXIGATE_OPS(xdev, idx)			\
	((struct xocl_axigate_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_AXIGATE, \
	idx).ops)
#define AXIGATE_CB(xdev, idx, cb)		\
	(AXIGATE_DEV(xdev, idx) && AXIGATE_OPS(xdev, idx) &&		\
	AXIGATE_OPS(xdev, idx)->cb)
#define xocl_axigate_freeze(xdev, level)		\
	(AXIGATE_CB(xdev, level, freeze) ?		\
	AXIGATE_OPS(xdev, level)->freeze(AXIGATE_DEV(xdev, level)) :	\
	-ENODEV)
#define xocl_axigate_free(xdev, level)		\
	(AXIGATE_CB(xdev, level, free) ?		\
	AXIGATE_OPS(xdev, level)->free(AXIGATE_DEV(xdev, level)) :	\
	-ENODEV)

struct xocl_mailbox_versal_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*set)(struct platform_device *pdev, u32 data);
	int (*get)(struct platform_device *pdev, u32 *data);
};
#define	MAILBOX_VERSAL_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MAILBOX_VERSAL).pldev
#define	MAILBOX_VERSAL_OPS(xdev)	\
	((struct xocl_mailbox_versal_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_MAILBOX_VERSAL).ops)
#define MAILBOX_VERSAL_READY(xdev, cb)	\
	(MAILBOX_VERSAL_DEV(xdev) && MAILBOX_VERSAL_OPS(xdev) &&	\
	 MAILBOX_VERSAL_OPS(xdev)->cb)
#define	xocl_mailbox_versal_set(xdev, data)	\
	(MAILBOX_VERSAL_READY(xdev, set) ?	\
	MAILBOX_VERSAL_OPS(xdev)->set(MAILBOX_VERSAL_DEV(xdev), \
	data) : -ENODEV)
#define	xocl_mailbox_versal_get(xdev, data)	\
	(MAILBOX_VERSAL_READY(xdev, get)	\
	? MAILBOX_VERSAL_OPS(xdev)->get(MAILBOX_VERSAL_DEV(xdev), \
	data) : -ENODEV)

static inline void __iomem *xocl_cdma_addr(xdev_handle_t xdev)
{
	void	__iomem *ioaddr;
	static uint32_t cdma[4];

	cdma[0] = (uint32_t)xocl_iores_get_offset(xdev, IORES_KDMA);
	if (cdma[0] != (uint32_t)-1)
		ioaddr = cdma;
	else
		ioaddr = xocl_rom_cdma_addr(xdev);

	return ioaddr;
}
/* helper functions */
xdev_handle_t xocl_get_xdev(struct platform_device *pdev);
void xocl_init_dsa_priv(xdev_handle_t xdev_hdl);

/* subdev mbx messages */
#define XOCL_MSG_SUBDEV_VER	1
#define XOCL_MSG_SUBDEV_DATA_LEN	(512 * 1024)

enum {
	XOCL_MSG_SUBDEV_RTN_UNCHANGED = 1,
	XOCL_MSG_SUBDEV_RTN_PARTIAL,
	XOCL_MSG_SUBDEV_RTN_COMPLETE,
};

/* subdev functions */
int xocl_subdev_init(xdev_handle_t xdev_hdl);
void xocl_subdev_fini(xdev_handle_t xdev_hdl);
int xocl_subdev_create(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info);
int xocl_subdev_create_by_id(xdev_handle_t xdev_hdl, int id);
int xocl_subdev_create_all(xdev_handle_t xdev_hdl);
void xocl_subdev_destroy_all(xdev_handle_t xdev_hdl);
int xocl_subdev_offline_all(xdev_handle_t xdev_hdl);
int xocl_subdev_offline_by_id(xdev_handle_t xdev_hdl, u32 id);
int xocl_subdev_offline_by_level(xdev_handle_t xdev_hdl, int level);
int xocl_subdev_online_all(xdev_handle_t xdev_hdl);
int xocl_subdev_online_by_id(xdev_handle_t xdev_hdl, u32 id);
int xocl_subdev_online_by_level(xdev_handle_t xdev_hdl, int level);
void xocl_subdev_destroy_by_id(xdev_handle_t xdev_hdl, u32 id);
void xocl_subdev_destroy_by_level(xdev_handle_t xdev_hdl, int level);

int xocl_subdev_create_by_name(xdev_handle_t xdev_hdl, char *name);
int xocl_subdev_destroy_by_name(xdev_handle_t xdev_hdl, char *name);

int xocl_subdev_destroy_prp(xdev_handle_t xdev);
int xocl_subdev_create_prp(xdev_handle_t xdev);

void xocl_fill_dsa_priv(xdev_handle_t xdev_hdl, struct xocl_board_private *in);
int xocl_xrt_version_check(xdev_handle_t xdev_hdl,
	struct axlf *bin_obj, bool major_only);
int xocl_alloc_dev_minor(xdev_handle_t xdev_hdl);
void xocl_free_dev_minor(xdev_handle_t xdev_hdl);

int xocl_ioaddr_to_baroff(xdev_handle_t xdev_hdl, resource_size_t io_addr,
	int *bar_idx, resource_size_t *bar_off);

static inline void xocl_lock_xdev(xdev_handle_t xdev)
{
	mutex_lock(&XDEV(xdev)->lock);
}

static inline void xocl_unlock_xdev(xdev_handle_t xdev)
{
	mutex_unlock(&XDEV(xdev)->lock);
}

static inline uint32_t xocl_dr_reg_read32(xdev_handle_t xdev, void __iomem *addr)
{
	u32 val;

	read_lock(&XDEV(xdev)->rwlock);
	val = ioread32(addr);
	read_unlock(&XDEV(xdev)->rwlock);

	return val;
}

static inline void xocl_dr_reg_write32(xdev_handle_t xdev, u32 value, void __iomem *addr)
{
	read_lock(&XDEV(xdev)->rwlock);
	iowrite32(value, addr);
	read_unlock(&XDEV(xdev)->rwlock);
}

/* context helpers */
extern struct mutex xocl_drvinst_mutex;
extern struct xocl_drvinst *xocl_drvinst_array[XOCL_MAX_DEVICES * 10];

void *xocl_drvinst_alloc(struct device *dev, u32 size);
void xocl_drvinst_free(void *data);
void *xocl_drvinst_open(void *file_dev);
void *xocl_drvinst_open_single(void *file_dev);
void xocl_drvinst_close(void *data);
void xocl_drvinst_set_filedev(void *data, void *file_dev);
void xocl_drvinst_offline(xdev_handle_t xdev_hdl, bool offline);
int xocl_drvinst_set_offline(void *data, bool offline);
int xocl_drvinst_get_offline(void *data, bool *offline);
int xocl_drvinst_kill_proc(void *data);

/* health thread functions */
int xocl_thread_start(xdev_handle_t xdev);
int xocl_thread_stop(xdev_handle_t xdev);

/* subdev blob functions */
int xocl_fdt_blob_input(xdev_handle_t xdev_hdl, char *blob);
int xocl_fdt_remove_subdevs(xdev_handle_t xdev_hdl, struct list_head *devlist);
int xocl_fdt_unlink_node(xdev_handle_t xdev_hdl, void *node);
int xocl_fdt_overlay(void *fdt, int target, void *fdto, int node, int pf);
int xocl_fdt_build_priv_data(xdev_handle_t xdev_hdl, struct xocl_subdev *subdev,
		void **priv_data,  size_t *data_len);
int xocl_fdt_get_userpf(xdev_handle_t xdev_hdl, void *blob);
int xocl_fdt_add_pair(xdev_handle_t xdev_hdl, void *blob, char *name,
		void *val, int size);
int xocl_fdt_get_next_prop_by_name(xdev_handle_t xdev_hdl, void *blob,
    int offset, char *name, const void **prop, int *prop_len);
int xocl_fdt_check_uuids(xdev_handle_t xdev_hdl, const void *blob,
		        const void *subset_blob);
const struct axlf_section_header *xocl_axlf_section_header(
	xdev_handle_t xdev_hdl, const struct axlf *top,
	enum axlf_section_kind kind);


/* init functions */
int __init xocl_init_userpf(void);
void xocl_fini_fini_userpf(void);

int __init xocl_init_drv_user_qdma(void);
void xocl_fini_drv_user_qdma(void);

int __init xocl_init_feature_rom(void);
void xocl_fini_feature_rom(void);

int __init xocl_init_xdma(void);
void xocl_fini_xdma(void);

int __init xocl_init_qdma(void);
void xocl_fini_qdma(void);

int __init xocl_init_mb_scheduler(void);
void xocl_fini_mb_scheduler(void);

int __init xocl_init_xvc(void);
void xocl_fini_xvc(void);

int __init xocl_init_firewall(void);
void xocl_fini_firewall(void);

int __init xocl_init_sysmon(void);
void xocl_fini_sysmon(void);

int __init xocl_init_mb(void);
void xocl_fini_mb(void);

int __init xocl_init_xiic(void);
void xocl_fini_xiic(void);

int __init xocl_init_mailbox(void);
void xocl_fini_mailbox(void);

int __init xocl_init_icap(void);
void xocl_fini_icap(void);

int __init xocl_init_mig(void);
void xocl_fini_mig(void);

int __init xocl_init_xmc(void);
void xocl_fini_xmc(void);

int __init xocl_init_dna(void);
void xocl_fini_dna(void);

int __init xocl_init_fmgr(void);
void xocl_fini_fmgr(void);

int __init xocl_init_mgmt_msix(void);
void xocl_fini_mgmt_msix(void);

int __init xocl_init_flash(void);
void xocl_fini_flash(void);

int __init xocl_init_axigate(void);
void xocl_fini_axigate(void);

int __init xocl_init_iores(void);
void xocl_fini_iores(void);

int __init xocl_init_mailbox_versal(void);
void xocl_fini_mailbox_versal(void);
#endif
