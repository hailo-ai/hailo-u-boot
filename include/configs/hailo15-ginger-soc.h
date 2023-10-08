/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO15_GINGER_SOC_H
#define __HAILO15_GINGER_SOC_H

#include "hailo15_common.h"

#undef COUNTER_FREQUENCY
#define COUNTER_FREQUENCY (15000000) // fpga xtal is 15mhz

#define PHYS_SDRAM_1_SIZE (0x80000000)

#endif /* __HAILO15_GINGER_SOC_H */
