// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 */

#include <common.h>
#include <asm/global_data.h>
#include <dm.h>
#include <dm/root.h>
#include <reset-uclass.h>
#include <scmi_base.h>
#include <hang.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
		gd->flags |= GD_FLG_SILENT;
	return 0;
}

int misc_init_r(void)
{
	struct udevice *scmi_agent_dev;
	u32 fw_version, impl_version;
	int ret;

	ret = uclass_first_device_err(UCLASS_SCMI_AGENT, &scmi_agent_dev);
	if (ret) {
		printf("Error retrieving SCMI agent uclass: ret=%d\n", ret);
		return ret;
	}

	ret = dev_read_u32(scmi_agent_dev, "fw-ver", &fw_version);
	if (ret) {
		printf("Error reading fw-ver from SCMI devicetree node: ret=%d\n", ret);
		return ret;
	}

	ret = scmi_base_discover_implementation_version(scmi_agent_dev, &impl_version);
	if (ret) {
		printf("Error getting SCMI implmentation version: ret=%d\n", ret);
		return ret;
	}

	if (fw_version != impl_version) {
		printf("Firmware version mismatch: expected=0x%x, actual=0x%x\n", fw_version, impl_version);
		return -EPERM;
	}

	return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
#if defined(CONFIG_HAILO15_DDR_ENABLE_ECC) && (CONFIG_NR_DRAM_BANKS > 1)
	gd->ram_size += PHYS_SDRAM_2_SIZE;
#endif
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
#if defined(CONFIG_HAILO15_DDR_ENABLE_ECC) && (CONFIG_NR_DRAM_BANKS > 1)
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
#endif
	return 0;
}

/*! @note This is a temporary implementation and should be removed once trusted-firmware will be used,
 *        and system-reset will be requested via PSCI request to the trusted-firmware.
 *		  Current implementation: system-reset request is done directly as OSPM agent via SCMI to SCU.
 */
void reset_cpu(ulong addr)
{
	int ret;
	struct reset_ctl reset;
	struct udevice *root = dm_root();

	ret = reset_get_by_name(root, "system-reset", &reset);
	if (ret) {
		printf("system-reset is not defined in DT root node, invoking hang() instead... ret[%d].\n", ret);
		goto do_hang_fn;
	}

	ret = reset_assert(&reset);
	if (ret) {
		printf("system-reset failed, invoking hang() instead: ret[%d].\n", ret);
	}

do_hang_fn:
	/*! @note if reset_assert succeed, then we will never reach to execute hang() function.
	 *        It is written here, just to make sure we hang if above code failed.
	 */
	hang();
}

#if defined(CONFIG_SHOW_BOOT_PROGRESS)
void show_boot_progress(int progress)
{
	printf("Boot reached stage %d\n", progress);
}
#endif
