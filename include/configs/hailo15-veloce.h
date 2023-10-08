/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO15_VELOCE_H
#define __HAILO15_VELOCE_H

#include "hailo15_common.h"

/*! @note: lpddr4 inline ecc located at the top 1/8 of the referred CS. 
 *         In regards of using LPDDR4 setup of a single rank:
 *         If __not__ using ECC, then memory regions are:: 
 *           - 0x80000000 - 0x8fffffff: Bank #0 (256M = 0x10000000) reserved for fake flash
 *           - 0x90000000 - 0x9fffffff: Bank #0 (256M = 0x10000000)
 *         If using ECC, then memory region range: 
 *           - 0x80000000 - 0x8fffffff: Bank #0     (256M = 0x10000000) reserved for fake flash
 *           - 0x90000000 - 0x9bffffff: Bank #0     (192M = 0xC000000)
 *           - 0x9c000000 - 0x9fffffff: Bank #0 ECC (64M  = 0x4000000)
 */

#undef PHYS_SDRAM_1
#define PHYS_SDRAM_1 (0x90000000)

#ifdef CONFIG_HAILO15_DDR_ENABLE_ECC
#define PHYS_SDRAM_1_SIZE (0xC000000)
#else
#define PHYS_SDRAM_1_SIZE (0x10000000)
#endif

#endif /* __HAILO15_VELOCE_H */
