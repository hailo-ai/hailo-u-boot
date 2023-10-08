/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO15_EVB_2_CAMERA_VPU_H
#define __HAILO15_EVB_2_CAMERA_VPU_H

#include "hailo15_common.h"

/*! @note: lpddr4 inline ecc located at the top 1/8 of the referred CS.
 *         In regards of using LPDDR4 setup of:
 *           - 2 ranks (Also refered as CS)
 *           - 2 channels per rank
 *           - Each channel is 16 bits wide => each rank is 32 bits bide
 *           - Rank size: 2G bytes
 *         If __not__ using ECC, then memory access are located in a single region:
 *           - 0x80000000 -  0x17fffffff: Bank #0 (4G = 0x100000000)
 *         If using ECC, then memory region is spilted to 2 ranges:
 *           - 0x080000000 - 0x0efffffff: Bank #0     (1.75G = 0x70000000)
 *           - 0x0f0000000 - 0x0ffffffff: Bank #0 ECC (0.25G = 0x10000000)
 *           - 0x100000000 - 0x16fffffff: Bank #1     (1.75G = 0x70000000)
 *           - 0x170000000 - 0x17fffffff: Bank #1 ECC (0.25G = 0x10000000)
 */
#ifdef CONFIG_HAILO15_DDR_ENABLE_ECC

/* Bank 0 size using ECC */
#define PHYS_SDRAM_1_SIZE (0x70000000)
/* Bank 1 address/size using ECC */
#define PHYS_SDRAM_2 (0x100000000)
#define PHYS_SDRAM_2_SIZE (0x70000000)

#else

/* Bank 0 size not using ECC */
#define PHYS_SDRAM_1_SIZE (0x100000000)

#endif

#endif /* __HAILO15_EVB_2_CAMERA_VPU_H */