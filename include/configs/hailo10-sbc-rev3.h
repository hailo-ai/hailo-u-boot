/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO10_SBC_REV3_H
#define __HAILO10_SBC_REV3_H

#ifdef CONFIG_SPL_BUILD

    #define CONFIG_EXTRA_ENV_SETTINGS \
        "default_spl_boot_source=nor\0" \
        "spl_boot_source=nor\0"

#endif /* CONFIG_SPL_BUILD */

#include "hailo15_common.h"

/*! @note: lpddr4 inline ecc located at the top 1/8 of the referred CS.
 *         In regards of using LPDDR4 setup of:
 *           - 1 ranks (Also refered as CS)
 *           - 2 channels per rank
 *           - Each channel is 16 bits wide => each rank is 32 bits bide
 *           - Rank size: 2G bytes
 *         If __not__ using ECC, then memory access are located in a single region:
 *           - 0x80000000 -  0xffffffff : Bank #0 (2G = 0x80000000)
 *         If using ECC, then memory access are located in a single region:
 *           - 0x080000000 - 0x0efffffff: Bank #0     (1.75G = 0x70000000)
 *           - 0x0f0000000 - 0x0ffffffff: Bank #0 ECC (0.25G = 0x10000000)
 */

#endif /* __HAILO10_SBC_REV3_H */