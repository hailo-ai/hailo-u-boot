/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for veloce Hailo15L.
 */

#ifndef __HAILO15L_VELOCE_H
#define __HAILO15L_VELOCE_H

#define SPL_BOOT_SOURCE "ram"

#define BOOTMENU \
    "default_spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "boot_ram_size=0x5000000\0"

#ifdef CONFIG_SPL_BUILD

#define CONFIG_EXTRA_ENV_SETTINGS \
    BOOTMENU

#endif /* CONFIG_SPL_BUILD */

#include "hailo15l_common.h"

#endif /* __HAILO15L_VELOCE_H */
