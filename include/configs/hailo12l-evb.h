/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 *
 * Configuration for Hailo12L EVB.
 */

#ifndef __HAILO12L_BOARD_H
#define __HAILO12L_BOARD_H

#define BOOTMENU_COMMON "bootargs_board_options=\"swiotlb=noforce\"\0"

#define BOOTMENU \
    BOOTMENU_COMMON \
    "default_spl_boot_source=ram\0" \
    "spl_boot_source=ram\0" \
    "boot_ram_size=0x10000000\0" \
    "bootmenu_0=Boot from RAM=run boot_ram\0"

#ifdef CONFIG_SPL_BUILD

    #define CONFIG_EXTRA_ENV_SETTINGS \
        BOOTMENU

#endif /* CONFIG_SPL_BUILD */

#include "hailo12l_common.h"

#endif /* __HAILO12L_BOARD_H */
