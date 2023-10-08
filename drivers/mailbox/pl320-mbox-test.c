// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 */

#include <common.h>
#include <log.h>
#include <dm.h>
#include <mailbox.h>
#include <malloc.h>
#include <asm/io.h>

struct pl320_mailbox_test {
	struct mbox_chan chan;
};

int pl320_mailbox_test_get(struct udevice *dev)
{
	struct pl320_mailbox_test *mb_test = dev_get_priv(dev);

	return mbox_get_by_name(dev, "a53-scu", &mb_test->chan);
}

int pl320_mailbox_test_send(struct udevice *dev, uint32_t msg)
{
	struct pl320_mailbox_test *mb_test = dev_get_priv(dev);

	return mbox_send(&mb_test->chan, &msg);
}

int pl320_mailbox_test_recv(struct udevice *dev, uint32_t *msg)
{
	struct pl320_mailbox_test *mb_test = dev_get_priv(dev);

	return mbox_recv(&mb_test->chan, msg, 100);
}

int pl320_mailbox_test_free(struct udevice *dev)
{
	struct pl320_mailbox_test *mb_test = dev_get_priv(dev);

	return mbox_free(&mb_test->chan);
}

static int pl320_mailbox_test_bind(struct udevice *dev)
{
	log_info("%s(dev=%p) name[%s]\n", __func__, dev, dev->name);

	return 0;
}

static const struct udevice_id pl320_mailbox_test_ids[] = {
	{ .compatible = "pl320-mailbox-test" },
	{ }
};

U_BOOT_DRIVER(pl320_mailbox_test) = {
	.name = "pl320_mailbox_test",
	.id = UCLASS_MAILBOX,
	.bind = pl320_mailbox_test_bind,	
	.of_match = pl320_mailbox_test_ids,
	.priv_auto = sizeof(struct pl320_mailbox_test),
};

