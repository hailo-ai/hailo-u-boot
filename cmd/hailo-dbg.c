// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/root.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <mailbox.h>
#include <reset-uclass.h>
#include <clk-uclass.h>
#include <linux/kernel.h>


/*!
 * @enum reset ctrl values
 */
enum reset_ctrl_e {
	RESET_CTRL_ASSERT,
	RESET_CTRL_DEASSERT
};

/*!
 * @enum reset ctrl values
 */
enum clk_ctrl_e {
	CLK_CTRL_START,
	CLK_CTRL_STOP
};


/*!
 * @brief DT property names for a specific UCLASS_XXX struct.
 *        Used to scan clients and their phandle udevice.
 */
struct dt_properties_names_t {
	enum uclass_id uclass_id;
	char *phandle_list_name;        /* DT property name of phandle list_name.      E.g.: "mboxes",      "resets"       */
	char *phandle_cells_name;       /* DT property name of phandle aruments count. E.g.: "#mbox-cells", "#reset-cells" */
	char *client_name;              /* DT property name of a client name.          E.g.: "mbox-names",  "reset-names"  */ 
};

static struct dt_properties_names_t g_db[] = {
		{UCLASS_MAILBOX, "mboxes", "#mbox-cells",  "mbox-names" }, 
		{UCLASS_RESET,   "resets", "#reset-cells", "reset-names"},
		{UCLASS_CLK,   "clocks", "#clock-cells", "clock-names"},

		/*! @note Do not remove it !!! */
		{UCLASS_INVALID}
};


/*!
 * @brief Resolve DT property names for a specific UCLASS_XXX.
 *
 * @param [in ]: uclass_id   - uclass_id
 * @param [out]: dt_pnames   - DT properties names appropriate for a specific UCLASS_XXX
 * 
 * @return 0(ok), fail(otherwise)
 */
int get_dt_properties_names(enum uclass_id uclass_id, struct dt_properties_names_t **dt_pnames) 
{
	struct dt_properties_names_t *entry = g_db;
	
	while (entry->uclass_id != UCLASS_INVALID) {
		if (entry->uclass_id == uclass_id) {
			*dt_pnames = entry;
			return 0;
		}
		entry++;
	}
	return -EINVAL;
}


/*!
 * @brief Show client by index of a specific udevice.
 * @note Auxilary function.
 *
 * @param [in] dev       - udevice node which is an mbox client 
 * @param [in] index     - client index 
 * @param [in] dt_pnames - a pointer to a DT properties names appropriate for a specific UCLASS_XXX.
 *
 * @return 0(ok), otherwise(fail).
 */
static int show_client_by_index(struct udevice *dev, int index, struct dt_properties_names_t *dt_pnames)
{
	struct ofnode_phandle_args args;
	struct udevice *phandle_dev;
	const char *client_name = "unknown";
	int ret;

	ret = dev_read_phandle_with_args(dev, dt_pnames->phandle_list_name, dt_pnames->phandle_cells_name, 
			0, index, &args);
	if (ret) {
		printf("%s (dev[%s], idx[%d], dt_pnames{uclass-id[%d], list-name[%s], list-cells[%s], client-name[%s]}): Failed to get phandle with args.\n", 
		__func__, dev->name, index, dt_pnames->uclass_id, dt_pnames->phandle_list_name, dt_pnames->phandle_cells_name, dt_pnames->client_name);
		return ret;
	}

	ret = uclass_get_device_by_ofnode(dt_pnames->uclass_id, args.node, &phandle_dev);
	if (ret) {
		/* Test with parent node */
		ret = uclass_get_device_by_ofnode(dt_pnames->uclass_id, ofnode_get_parent(args.node), &phandle_dev);
		if (ret) {
			printf("%s function (dev[%s], idx[%d], uclass_id[%d]): Failed to get phandle node.\n", 
			__func__, dev->name, index, dt_pnames->uclass_id);
			return ret;
		}
	}

	ret = dev_read_string_index(dev, dt_pnames->client_name, index, &client_name);
	if (ret) {
		printf("%s function (dev[%s], idx[%d], uclass_id[%d]): Failed to read property[%s].\n", 
			__func__, dev->name, index, dt_pnames->uclass_id, dt_pnames->client_name);
		return ret;
	}
	printf("%-20s %-20s %-20s %-20s\n", client_name, dev->name, phandle_dev->name, phandle_dev->driver->name);

	return 0;
}


/*!
 * @brief List all clients of a udevice node and its childs recursively
 * @note  Auxilary function
 *
 * @param [in] dev       - udevice node
 * @param [in] dt_pnames - a pointer to a DT properties names appropriate for a specific UCLASS_XXX.
 *
 * @return none
 */
static void list_dev_clients(struct udevice *dev, struct dt_properties_names_t *dt_pnames)
{
	struct udevice *child;
	int i, ret, cnt;

	cnt = dev_read_string_count(dev, dt_pnames->client_name);
	for(i = 0; i < cnt; i++) {
		ret = show_client_by_index(dev, i, dt_pnames);
		if (ret) {
			break;
		}
	}

	/* find mbox clients recursively */
	list_for_each_entry(child, &dev->child_head, sibling_node) {
		list_dev_clients(child, dt_pnames);
	}
}


/*!
 * @brief list all clients and their phandle udevice of a specific UCLASS_XXX
 *
 * @param [in] uclass_id - UCLASS_XXX id
 * 
 * @return none
 */
static void list_clients(enum uclass_id uclass_id)
{
	struct udevice *root;
	struct dt_properties_names_t *dt_pnames;
	int ret;
	char *table_hdr_sep = "--------------------";

	printf("%-20s %-20s %-20s %-20s\n", "client name", "client device", "phandle device", "phandle driver");
	printf("%-20s %-20s %-20s %-20s\n", table_hdr_sep, table_hdr_sep, table_hdr_sep, table_hdr_sep);
	root = dm_root();
	if(!root) {
		printf("%s(uclass_id[%d]): failed, root node not found.\n", __func__, uclass_id);
		return;
	}

	ret = get_dt_properties_names(uclass_id, &dt_pnames);
	if (ret) {
		printf("%s(uclass_id[%d]): failed, unsupported specified UCALSS_ID.\n", __func__, uclass_id);
		return;
	}

	list_dev_clients(root, dt_pnames);
}


/*!
 * @brief find client udevice of a specific client name.
 *
 * @param [in ] dev     - udevice node tree
 * @param [in ] name    - client name to find as specified in DT
 * @param [out] out_dev - pointer address to return client udevice.
 *
 * @return none
 */
static void find_client_dev_by_name(struct udevice *dev, const char *name, enum uclass_id uclass_id, struct udevice **out_dev)
{
	struct dt_properties_names_t *dt_pnames;
	struct udevice *child;
	int i, cnt, ret;

	ret = get_dt_properties_names(uclass_id, &dt_pnames);
	if (ret) {
		printf("%s(dev[%s], client_name[%s], uclass_id[%d]): failed, unsupported specified UCALSS_ID.\n", 
		__func__, dev->name, name, uclass_id);
		return;
	}

	cnt = dev_read_string_count(dev, dt_pnames->client_name);
	for (i = 0; i < cnt; i++) {
		const char *client_name;
		ret = dev_read_string_index(dev, dt_pnames->client_name, i, &client_name);
		if (ret) {
			continue;
		}
		if (0 == strcmp(name, client_name)) {
			*out_dev = dev;
			return;
		}
	}

	/* Search udevice childs recursively. */
	list_for_each_entry(child, &dev->child_head, sibling_node) {
		find_client_dev_by_name(child, name, uclass_id, out_dev);
		if (*out_dev != NULL) {
			return;
		}
	}
}

#ifdef CONFIG_DM_MAILBOX

/*!
 * @brief simple named mbox channel test with a test status result indication
 *
 * @param [in] chan_name - mbox channel name to find
 *
 * @return none
 */
static void mbox_client_send_test(char *chan_name)
{
	struct udevice *root, *dev = NULL;
	struct mbox_chan chan;
	uint32_t msg = 0xaaff9955UL;
	int ret;

	root = dm_root();
	if(!root) {
		printf("Failed to get a reference to root udevice object, aborting...\n");
		return;
	}

	/* find mbox client udevice with a specific mbox channel name */
	find_client_dev_by_name(root, chan_name, UCLASS_MAILBOX, &dev);
	if (!dev) {
		printf("mbox channel %s: not exist\n", chan_name);
		return;
	}

	ret = mbox_get_by_name(dev, chan_name, &chan);
	if (ret) {
		printf("mbox channel %s test: failed to get a reference to channel object, ret[%d]\n", 
			chan_name, ret);
		return;
	}

	ret = mbox_send(&chan, &msg);
	if (ret) {
		printf("mbox channel %s test: failed on send, ret[%d]\n", chan_name, ret);
		return;
	}

	ret = mbox_free(&chan);
	if (ret) {
		printf("mbox channel %s test: failed on free channel object, ret[%d]\n", chan_name, ret);
		return;
	}

	printf("mbox channel %s test: pass\n", chan_name);
}

#endif /* CONFIG_DM_MAILBOX */

#ifdef CONFIG_DM_RESET

/*!
 * @brief reset assert/de-asseert client by DT name
 *
 * @param [in] name - reset client name as specified in DT
 * @param [in] ctrl - enum reset_ctrl_e.
 *
 * @return none
 */
static void reset_client_ctrl(char *client_name, enum reset_ctrl_e ctrl)
{
	struct udevice *root, *dev = NULL;
	struct reset_ctl reset;
	int ret;

	root = dm_root();
	if(!root) {
		printf("Failed to get a reference to root udevice object, aborting...\n");
		return;
	}

	/* find mbox client udevice with a specific mbox channel name */
	find_client_dev_by_name(root, client_name, UCLASS_RESET, &dev);
	if (!dev) {
		return;
	}

	ret = reset_get_by_name(dev, client_name, &reset);
	if (!ret) {
		switch (ctrl) {
		case RESET_CTRL_ASSERT:
			printf("Client %s: reset assert\n", client_name);
			ret = reset_assert(&reset);
			if (ret) {
				printf("Client %s: reset assert, failed. ret[%d]\n", client_name, ret);
				return;
			}
			break;
		case RESET_CTRL_DEASSERT:
			printf("Client %s: reset de-assert\n", client_name);
			ret = reset_deassert(&reset);
			if (ret) {
				printf("Client %s: reset de-assert, failed. ret[%d]\n", client_name, ret);
				return;
			}
			break;
		default:
			printf("Reset client %s: unknown reset ctrl value[%d]\n", client_name, ctrl);
		}
	}
}

#endif /* CONFIG_DM_RESET */

#ifdef CONFIG_CLK

/*!
 * @brief clk start/stop client by DT name
 *
 * @param [in] name - clock client name as specified in DT
 * @param [in] ctrl - enum clk_ctrl_e.
 *
 * @return none
 */
static void clock_client_ctrl(char *client_name, enum clk_ctrl_e ctrl)
{
	struct udevice *root, *dev = NULL;
	//struct reset_ctl reset;
	struct clk clk;
	int ret;

	root = dm_root();
	if(!root) {
		printf("Failed to get a reference to root udevice object, aborting...\n");
		return;
	}

	/* find mbox client udevice with a specific mbox channel name */
	find_client_dev_by_name(root, client_name, UCLASS_CLK, &dev);
	if (!dev) {
		return;
	}

	ret = clk_get_by_name(dev, client_name, &clk);
	if (!ret) {
		switch (ctrl) {
		case CLK_CTRL_START:
			printf("Client %s: clock start\n", client_name);
			ret = clk_enable(&clk);
			if (ret) {
				printf("Client %s: clock start, failed. ret[%d]\n", client_name, ret);
				return;
			}
			break;
		case CLK_CTRL_STOP:
			printf("Client %s: clock stop\n", client_name);
			ret = clk_disable(&clk);
			if (ret) {
				printf("Client %s: clock stop, failed. ret[%d]\n", client_name, ret);
				return;
			}
			break;
		default:
			printf("clock client %s: unknown clock ctrl value[%d]\n", client_name, ctrl);
		}
	}
}

#endif /* CONFIG_CLK */

/*!
 * @brief Main Hailo debug cli command parser
 */
static int do_hailo_dbg(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
#ifdef CONFIG_DM_MAILBOX	
	if (strcmp(argv[1], "mbox-list") == 0 && argc == 2) {
		list_clients(UCLASS_MAILBOX);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "mbox-send-test") == 0 && argc == 3) {
		mbox_client_send_test(argv[2]);
		return CMD_RET_SUCCESS;
	}
#endif /* CONFIG_DM_MAILBOX */	
#ifdef CONFIG_DM_RESET
	if (strcmp(argv[1], "reset-list") == 0 && argc == 2) {
		list_clients(UCLASS_RESET);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "reset-assert") == 0 && argc == 3) {
		reset_client_ctrl(argv[2], RESET_CTRL_ASSERT);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "reset-deassert") == 0 && argc == 3) {
		reset_client_ctrl(argv[2], RESET_CTRL_DEASSERT);
		return CMD_RET_SUCCESS;
	}
#endif /* CONFIG_DM_RESET */	
#ifdef CONFIG_CLK
	if (strcmp(argv[1], "clk-list") == 0 && argc == 2) {
		list_clients(UCLASS_CLK);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "clk-start") == 0 && argc == 3) {
		clock_client_ctrl(argv[2], CLK_CTRL_START);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "clk-stop") == 0 && argc == 3) {
		clock_client_ctrl(argv[2], CLK_CTRL_STOP);
		return CMD_RET_SUCCESS;
	}
#endif /* CONFIG_CLK */	

    return CMD_RET_USAGE;
}


#ifdef CONFIG_SYS_LONGHELP
static char hailo_dbg_help_text[] =
	"[op]\n"
#ifdef CONFIG_DM_MAILBOX
	"hailodbg mbox-list                     - list all mbox clients\n"
	"hailodbg mbox-send-test <client-name>  - run a simple send test on a specific named mbox client\n"
#endif /* CONFIG_DM_MAILBOX */
#ifdef CONFIG_DM_RESET
	"hailodbg reset-list                    - list all reset clients\n"
	"hailodbg reset-assert <client-name>    - reset client assert\n"
	"hailodbg reset-deassert <client-name>  - reset client de-assert\n"
#endif /* CONFIG_DM_RESET */	
#ifdef CONFIG_CLK
	"hailodbg clk-list                     - list all clock clients\n"
	"hailodbg clk-start <client-name>      - clock client start\n"
	"hailodbg clk-stop  <client-name>      - clock client stop\n";
#endif /* CONFIG_CLK */	
#endif /* CONFIG_SYS_LONGHELP */

U_BOOT_CMD(
	hailodbg,    255,    0,    do_hailo_dbg,
	"hailo debbug commands",
	hailo_dbg_help_text
);
