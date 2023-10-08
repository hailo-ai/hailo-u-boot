// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 */

/*!
 * @brief DTS mailbox example
 *
 *	--------------------- mailbox controller ----------------------
 *	note: 
 *    - #mbox-cells: represents number of client mbox params: <dst-chan, dst-mbox, src-mbox>
 *    - arm,dev-ch-idx: device source channel id
 *    - 
 *
 *	pl320_mailbox: mailbox@123456 {
 *        #mbox-cells = <4>;
 *        compatible = "arm,pl320-mailbox";
 *        reg = <0 0x123456 0 0x1000>;
 *        arm,dev-ch-idx = <0>
 *	};
 *
 *	--------------------- mailbox client ----------------------
 *	mboxes list format:
 *	- Mboxes = <client-1-desc  client-2-desc  ...  client-n-desc>  
 *	- Each client is composed from 5 params: 
 *	  1) mbox-phandle
 *	  2) dst-chan  
 *	  3) dst-mbox 
 *	  4) src-mbox
 *	 
 *	E.g.: defining 2 clients which uses the same PL320 mailbox controller:
 *	  ------------- ------------------------------------------------
 *	 | client name | mbox ctrl     | src mbox | dst mbox | dst chan |
 *	 |-------------|------------------------------------------------|
 *	 | client1     | pl320_mailbox |  0       |  1       | 6        |
 *	 | client2     | pl320_mailbox |  2       |  3       | 7        |
 *	  ------------- ------------------------------------------------
 *	 
 *	mbox-client-node {
 *        compatible = "pl320-mailbox-test";
 *        mbox-names = "client1", "client2";
 *        mboxes = <&pl320_mailbox 6 1 0 &pl320_mailbox 7 3 2>;
 *	};
 */


#include <common.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <dm.h>
#include <mailbox-uclass.h>

/*!
 *	@brief PL320 IPC mailbox regs, m=(mailbox index).
*/
#define IPCMxSOURCE(m)      ( (m) * 0x40)
#define IPCMxDSET(m)        (((m) * 0x40) + 0x004)                  /* Destination Set reg */
#define IPCMxDCLEAR(m)      (((m) * 0x40) + 0x008)                  /* Destination Clear reg */
#define IPCMxDSTATUS(m)     (((m) * 0x40) + 0x00C)                  /* Destination Status reg */
#define IPCMxMODE(m)        (((m) * 0x40) + 0x010)                  /* Mode reg */
#define IPCMxMSET(m)        (((m) * 0x40) + 0x014)                  /* Mask Set reg */
#define IPCMxMCLEAR(m)      (((m) * 0x40) + 0x018)                  /* Mask Clear reg */
#define IPCMxMSTATUS(m)     (((m) * 0x40) + 0x01C)                  /* Mask Status reg */
#define IPCMxSEND(m)        (((m) * 0x40) + 0x020)                  /* Send reg */
#define IPCMxDR(m, dr)      (((m) * 0x40) + ((dr) * 4) + 0x024)     /* Data regs */

/*!
 *	@brief PL320 IPC interrupt regs
*/
#define IPCMMIS(irq)        (((irq) * 8) + 0x800)                   /* Masked Interrupt Status register */
#define IPCMRIS(irq)        (((irq) * 8) + 0x804)                   /* Raw    Interrupt Status register */

#define MBOX_MASK(n)        (1UL << (n))
#define CHAN_MASK(n)        (1UL << (n))

#define DEBUG_CHANNEL(chan_info) \
	log_debug("%s[%d]: src(mbox[%u]), dst(mbox[%u] chan[%u])\n", __func__, __LINE__, \
		chan_info->src_mbox_id,	chan_info->dst_mbox_id, chan_info->dst_chan_id) 

/*! 
 * @brief Maximum mailbox send retries.
 */
#define MAX_SEND_RETRIES 1000000

/*!
 *	@brief PL320 IPC interrupt IPCMxSEND(m) send message cmd types. 
*/
typedef enum _mbox_cmd_e {
	MBOX_SEND_CLEAR = 0x0,
	MBOX_SEND_DESTINATION = 0x1,
	MBOX_SEND_SOURCE = 0x2,
} mbox_cmd_e;

/**
 * Hailo15 PL320 Mailbox device data
 *
 * @dev:         A reference to udevice object which this info describes
 * @regs:        PL320 registers mapping BAR
 * @src_chan_id: src channel id for rx msg doorbell
 */
struct pl320_mailbox_dev_info {
	struct udevice *dev;
	void __iomem *regs;
	uint32_t src_chan_id;
	
};

/**
 * Hailo15 PL320 Mailbox channel data
 *
 * @src_mbox_id: src mailbox id for Tx data
 * @dst_mbox_id: dst mailbox id for Rx data
 * @dst_chan_id: dst channel id for dst unit doorbell
 */
struct pl320_mailbox_channel_info {
	uint32_t src_mbox_id;
	uint32_t dst_mbox_id;
	uint32_t dst_chan_id;
};

/* Max number of supported mailboxes & channels. */
#define MAX_NUM_OF_CHANNELS (8)
#define MAX_NUM_OF_MBOXES (16)


/*!
 * @brief: basic mbox read/write regs functions
 */

/* calculate reg address */
static inline uint32_t *pl320_mailbox_reg(struct pl320_mailbox_dev_info *mb, uint32_t reg)
{
	return (uint32_t *)(mb->regs + reg);
}
/* Read reg */
static inline uint32_t pl320_mailbox_read_u32(struct pl320_mailbox_dev_info *mb, uint32_t reg)
{
	return readl_relaxed(pl320_mailbox_reg(mb, reg));
}
/* Write reg */
static inline void pl320_mailbox_write_u32(struct pl320_mailbox_dev_info *mb, uint32_t reg, uint32_t val)
{
	writel_relaxed(val, pl320_mailbox_reg(mb, reg));
}


/*!
 * @brief mbox_ops functions implementation
 */

/* mbox_ops::of_xlate */
static int pl320_mailbox_of_xlate(struct mbox_chan *chan, struct ofnode_phandle_args *args)
{
	struct pl320_mailbox_channel_info *chan_info;

	log_debug("%s(chan=%p)\n", __func__, chan);

	if (args->args_count != 3) {
		log_err("Invaild args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	/* Validate Channel destination ID */
	if (args->args[0] >= MAX_NUM_OF_CHANNELS) {
		log_err("Invaild channel dst id: %u\n", args->args[0]);
		return -EINVAL;
	}

	/* Validate Mailbox destination ID */
	if (args->args[1] >= MAX_NUM_OF_MBOXES) {
		log_err("Invaild mailbox dst id: %u\n", args->args[1]);
		return -EINVAL;
	}

	/* Validate Mailbox source ID */
	if (args->args[2] >= MAX_NUM_OF_MBOXES) {
		log_err("Invaild mailbox src id: %u\n", args->args[2]);
		return -EINVAL;
	}

	chan_info = calloc(1, sizeof(struct pl320_mailbox_channel_info));
	if (chan_info == NULL)
		return -ENOMEM;

	chan_info->dst_chan_id = args->args[0];
	chan_info->dst_mbox_id = args->args[1];
	chan_info->src_mbox_id = args->args[2];

	DEBUG_CHANNEL(chan_info);

	chan->con_priv = chan_info;

	return 0;
}

/* mbox_ops::request */
static int pl320_mailbox_request(struct mbox_chan *chan)
{
	struct pl320_mailbox_dev_info *dev_info = (struct pl320_mailbox_dev_info *)dev_get_priv(chan->dev);
	struct pl320_mailbox_channel_info *chan_info = (struct pl320_mailbox_channel_info *)chan->con_priv;
	uint32_t source;

	DEBUG_CHANNEL(chan_info);

	/* Try to acquire src mailbox */
	pl320_mailbox_write_u32(dev_info, IPCMxSOURCE(chan_info->src_mbox_id), CHAN_MASK(dev_info->src_chan_id));
	source = pl320_mailbox_read_u32(dev_info, IPCMxSOURCE(chan_info->src_mbox_id));
	if (unlikely(source != CHAN_MASK(dev_info->src_chan_id))) {
		log_debug("%s[%d]: Mailbox %u already used by channel mask id %08x\n", __func__, __LINE__,
			chan_info->src_mbox_id, source);
		return -EBUSY;
	}
	/* Intialize dest mailbox */
	pl320_mailbox_write_u32(dev_info, IPCMxDSET(chan_info->src_mbox_id), CHAN_MASK(chan_info->dst_chan_id));
	// Mask src & dst channels
	pl320_mailbox_write_u32(dev_info, IPCMxMSET(chan_info->src_mbox_id), CHAN_MASK(dev_info->src_chan_id) | CHAN_MASK(chan_info->dst_chan_id));

	return 0;
}

/* mbox_ops::rfree */
static int pl320_mailbox_free(struct mbox_chan *chan)
{
	struct pl320_mailbox_dev_info *dev_info = (struct pl320_mailbox_dev_info *)dev_get_priv(chan->dev);
	struct pl320_mailbox_channel_info *chan_info = (struct pl320_mailbox_channel_info *)chan->con_priv;

	DEBUG_CHANNEL(chan_info);

	pl320_mailbox_write_u32(dev_info, IPCMxSOURCE(chan_info->src_mbox_id), 0);
	if (chan->con_priv) {
		free(chan->con_priv);
		chan->con_priv = NULL;
	}

	return 0;
}


/* mbox_ops::send */
static int pl320_mailbox_send(struct mbox_chan *chan, const void *data)
{
	struct pl320_mailbox_dev_info *dev_info = (struct pl320_mailbox_dev_info *)dev_get_priv(chan->dev);
	struct pl320_mailbox_channel_info *chan_info = (struct pl320_mailbox_channel_info *)chan->con_priv;

	DEBUG_CHANNEL(chan_info);

	pl320_mailbox_write_u32(dev_info, IPCMxSEND(chan_info->src_mbox_id), MBOX_SEND_DESTINATION);
	log_debug("%s[%d]: Send msg to dst channel index %d via src mailbox %d\n", 
		__func__, __LINE__, chan_info->dst_chan_id, chan_info->src_mbox_id);

	return 0;
}

/* mbox_ops::recv */
static int pl320_mailbox_recv(struct mbox_chan *chan, void *data)
{
	struct pl320_mailbox_dev_info *dev_info = (struct pl320_mailbox_dev_info *)dev_get_priv(chan->dev);
	struct pl320_mailbox_channel_info *chan_info = (struct pl320_mailbox_channel_info *)chan->con_priv;
	uint32_t mis;

	DEBUG_CHANNEL(chan_info);

	mis = pl320_mailbox_read_u32(dev_info, IPCMMIS(dev_info->src_chan_id));
	log_debug("%s[%d]: IPCMMIS(%d) = %08X\n", __func__, __LINE__, dev_info->src_chan_id, mis);
	if (mis & MBOX_MASK(chan_info->dst_mbox_id)) {
		/* clear interrupt */
		pl320_mailbox_write_u32(dev_info, IPCMxSEND(chan_info->dst_mbox_id), MBOX_SEND_CLEAR);
		return 0;
	}

	return -ENODATA;
}

/* struct driver::probe */
static int pl320_mailbox_probe(struct udevice *dev)
{
	struct pl320_mailbox_dev_info *mb_dev = dev_get_priv(dev);
	fdt_addr_t fdt_addr;

	dev_dbg(dev,"%s\n", __func__);

	mb_dev->dev = dev;
	fdt_addr = devfdt_get_addr(dev);
	if (fdt_addr == FDT_ADDR_T_NONE)
		return -ENODEV;

	mb_dev->regs = (void*)fdt_addr;
	dev_dbg(dev,"%s(fdtaddr=%llx)\n", __func__, fdt_addr);

	if (dev_read_u32(dev, "arm,dev-ch-idx", &mb_dev->src_chan_id)) {
		dev_err(dev, "probe failed: DT node property dev-ch-idx: missing/syntax error.\n");
		return -ENODEV;
	}

	if (mb_dev->src_chan_id >= MAX_NUM_OF_CHANNELS) {
		dev_err(dev, "probe failed: DT node property dev-ch-idx: invalid val[%u], max allowed[%u].\n", 
			mb_dev->src_chan_id, MAX_NUM_OF_CHANNELS);
		return -ENODEV;
	}

	return 0;
}

static const struct udevice_id pl320_mailbox_ids[] = {
	{ .compatible = "arm,pl320-mailbox" },
	{ }
};

struct mbox_ops pl320_mailbox_ops = {
	.of_xlate = pl320_mailbox_of_xlate,
	.request = pl320_mailbox_request,
	.rfree = pl320_mailbox_free,
	.send = pl320_mailbox_send,
	.recv = pl320_mailbox_recv,
};

U_BOOT_DRIVER(pl320_mailbox) = {
	.name = "pl320_mailbox",
	.id = UCLASS_MAILBOX,
	.of_match = pl320_mailbox_ids,
	.probe = pl320_mailbox_probe,
	.priv_auto = sizeof(struct pl320_mailbox_dev_info),
	.ops = &pl320_mailbox_ops,
};
