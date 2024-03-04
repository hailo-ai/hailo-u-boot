// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */

#include <common.h>
#include <spl.h>
#include <env.h>
#include <hang.h>
#include "hailo15_board.h"

void spl_board_init(void)
{
	if (hailo15_scmi_init()) {
		hang();
	}
	if (hailo15_scmi_check_version_match()) {
		hang();
	}
}

void board_boot_order(u32 *spl_boot_list)
{
	const char *s;

	env_init();
	env_load();

	s = env_get("spl_boot_source");
	if (!s) {
		puts("failed to get 'spl_boot_source' from env, falling back to mmc12\n");
		s = "mmc12";
	}

	if (!strcmp(s, "mmc1")) {
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
	} else if (!strcmp(s, "mmc2")) {
		spl_boot_list[0] = BOOT_DEVICE_MMC2;
	} else if (!strcmp(s, "mmc12")) {
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		spl_boot_list[1] = BOOT_DEVICE_MMC2;
	} else if (!strcmp(s, "mmc21")) {
		spl_boot_list[0] = BOOT_DEVICE_MMC2;
		spl_boot_list[1] = BOOT_DEVICE_MMC1;
	} else if (!strcmp(s, "uart")) {
		spl_boot_list[0] = BOOT_DEVICE_UART;
	} else {
		printf("spl_boot_source=%s unsupported, falling back to mmc12\n", s);
		s = "mmc12";
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		spl_boot_list[1] = BOOT_DEVICE_MMC2;
	}

	printf("U-Boot SPL boot source %s\n", s);
}

int spl_mmc_fs_boot_partition(void)
{
	return hailo15_mmc_boot_partition();
}
