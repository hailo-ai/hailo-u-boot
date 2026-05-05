/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 *
 * Configuration for Hailo12L Mint.
 */

#ifndef __HAILO12L_MINT_H
#define __HAILO12L_MINT_H

#define SPL_BOOT_SOURCE "ram"
// #define SPL_BOOT_SOURCE "nor"

#define BOOTMENU \
    "default_spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "boot_ram_size=0x5000000\0" \
    "bootmenu_0=Autodetect=" \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from SD; run boot_mmc0;" \
        "echo Trying boot from RAM; run boot_ram;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from SD=run boot_mmc0\0" \
    "bootmenu_2=Boot from RAM=run boot_ram\0" \
    "bootmenu_3=Boot from NFS=run bootnfs\0" \
    "bootdelay=4\0"

#ifdef CONFIG_SPL_BUILD

#define CONFIG_EXTRA_ENV_SETTINGS \
    BOOTMENU

#endif /* CONFIG_SPL_BUILD */

#include "hailo12l_common.h"

#undef COUNTER_FREQUENCY
#define COUNTER_FREQUENCY (15000000) // fpga xtal is 15mhz

#define PHYS_SDRAM_1_SIZE (0x80000000)

#endif /* __HAILO12L_MINT_H */
