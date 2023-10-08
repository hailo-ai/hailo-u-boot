// SPDX-License-Identifier: GPL-2.0+
/*
 * SDHCI ADMA2 helper functions.
 */

#include <common.h>
#include <cpu_func.h>
#include <sdhci.h>
#include <malloc.h>
#include <asm/cache.h>

void sdhci_adma_desc(struct sdhci_adma_desc **desc,
			    dma_addr_t addr, u16 len, bool end)
{
	u8 attr;
	struct sdhci_adma_desc *dma_desc = *desc;

	attr = ADMA_DESC_ATTR_VALID | ADMA_DESC_TRANSFER_DATA;
	if (end)
		attr |= ADMA_DESC_ATTR_END;

	dma_desc->attr = attr;
	dma_desc->len = len;
	dma_desc->reserved = 0;
	dma_desc->addr_lo = lower_32_bits(addr);
#ifdef CONFIG_DMA_ADDR_T_64BIT
	dma_desc->addr_hi = upper_32_bits(addr);
#endif
	(*desc)++;
}

/**
 * sdhci_prepare_adma_table() - Populate the ADMA table
 *
 * @table:	Pointer to the ADMA table
 * @data:	Pointer to MMC data
 * @addr:	DMA address to write to or read from
 *
 * Fill the ADMA table according to the MMC data to read from or write to the
 * given DMA address.
 * Please note, that the table size depends on CONFIG_SYS_MMC_MAX_BLK_COUNT and
 * we don't have to check for overflow.
 */
void __sdhci_prepare_adma_table(struct sdhci_adma_desc *table,
			      struct mmc_data *data, dma_addr_t addr, sdhci_adma_desc_func_t desc_func)
{
	uint trans_bytes = data->blocksize * data->blocks;
	int i = DIV_ROUND_UP(trans_bytes, ADMA_MAX_LEN);
	struct sdhci_adma_desc *desc = table;

	while (--i) {
		desc_func(&desc, addr, ADMA_MAX_LEN, false);
		addr += ADMA_MAX_LEN;
		trans_bytes -= ADMA_MAX_LEN;
	}

	desc_func(&desc, addr, trans_bytes, true);

	flush_cache((dma_addr_t)table,
		    ROUND(((uintptr_t)desc - (uintptr_t)table),
			  ARCH_DMA_MINALIGN));
}

/**
 * sdhci_adma_init() - initialize the ADMA descriptor table
 *
 * @return pointer to the allocated descriptor table or NULL in case of an
 * error.
 */
struct sdhci_adma_desc *__sdhci_adma_init(uint extra_desc)
{
	uint table_size = ADMA_TABLE_SZ + (extra_desc * ADMA_DESC_LEN);

	return memalign(ARCH_DMA_MINALIGN, table_size);
}
