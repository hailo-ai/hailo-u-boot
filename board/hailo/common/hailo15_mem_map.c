// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 */

#include <common.h>
#include <asm/armv8/mmu.h>

/* original */
static struct mm_region hailo15_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
#ifdef CONFIG_HAILO15_DDR_ENABLE_ECC
	{
		.virt = 0x080000000UL,
		.phys = 0x080000000UL,
		.size = 0x070000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
	{
		.virt = 0x100000000UL,
		.phys = 0x100000000UL,
		.size = 0x070000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
#else
	{
		.virt = 0x080000000UL,
		.phys = 0x080000000UL,
		.size = 0x100000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	},
#endif
	{
		/* List terminator */
		0
	}
};

struct mm_region *mem_map = hailo15_mem_map;
