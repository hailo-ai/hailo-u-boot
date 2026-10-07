// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/bitops.h>
#include <common.h>
#include <spl.h>
#include <env.h>
#include <hang.h>
#include <init.h>
#include <cpu_func.h>
#include <dm.h>
#include <dt-bindings/soc/hailo15_scmi_api.h>
#include "scmi_hailo.h"
#include "hailo15_board.h"
#include "scmi_hailo_protocol.h"
#ifdef CONFIG_SPL_ENV_IS_IN_MMC
#include "mmc.h"
#endif

// Global variable to indicate if uboot TFA is in mmc2 boot section, and should be loaded from there. 
// Variable is updated according to u-boot env variable "spl_boot_source"
int is_mmc2_boot = 0;

#define BASE_SPI_FLASH_ADDRESS 0x70000000

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_MACH_HAILO10

u32 hailo10_sku_board_id;
u32 hailo_sku_product_id;

int hailo_sku_info_get(void)
{
    struct udevice *scmi_agent_dev;
    struct scmi_hailo_get_sku_id_p2a sku_id;
    int ret;

    ret = uclass_first_device_err(UCLASS_SCMI_AGENT, &scmi_agent_dev);
    if (ret) {
        printf("Error retrieving SCMI agent uclass: ret=%d\n", ret);
        return ret;
    }

    ret = scmi_hailo_get_sku_id(scmi_agent_dev, &sku_id);
    if (ret) {
        printf("Error getting SKU ID: ret=%d\n", ret);
        return ret;
    }

    hailo_sku_product_id = sku_id.product;
    hailo10_sku_board_id = sku_id.board;
    if (hailo10_sku_board_id == HAILO10_SCMI_BOARD_SKU_ID__INVALID) {
        hailo10_sku_board_id = 0;
    }

    return 0;
}
#endif /* CONFIG_MACH_HAILO10 */

void board_init_f(ulong dummy)
{
    /* we enable MMU + caching before doing anything else to improve performace */
    gd->relocaddr = PHYS_SDRAM_1 + CONFIG_HAILO15_SPL_DRAM_SIZE;
    arch_reserve_mmu();
    icache_enable();
    dcache_enable();

    /* this part is the same as board_init_f in common/spl/spl.c */

    if (CONFIG_IS_ENABLED(OF_CONTROL)) {
        int ret;

        ret = spl_early_init();
        if (ret) {
            debug("spl_early_init() failed: %d\n", ret);
            hang();
        }
    }

       preloader_console_init();

}

void spl_board_init(void)
{
#ifdef CONFIG_SPL_ENV_IS_IN_MMC
    mmc_initialize(NULL);
#endif
    if (hailo15_scmi_init()) {
        hang();
    }
    if (hailo15_scmi_check_version_match()) {
        hang();
    }
#ifdef CONFIG_MACH_HAILO10    
    if (hailo_sku_info_get()) {
        hang();
    }
#endif /* CONFIG_MACH_HAILO10 */
}

void spl_board_prepare_for_boot(void)
{
#if CONFIG_IS_ENABLED(OS_BOOT)
    debug("Falcon mode: send boot success indication\n");
    if (hailo15_send_scmi_boot_success()) {
        hang();
    }
#endif
    dcache_disable();
    icache_disable();
}

void board_boot_order(u32 *spl_boot_list)
{
    const char *s;

    // Init indication that uboot TFA is in mmc boot section
    is_mmc2_boot = 0;

    env_init();
    env_load();

    s = env_get("spl_boot_source");
    if (!s) {
        puts("failed to get 'spl_boot_source' from env, falling back to mmc12\n");
        s = "mmc12";
    }

    if (!strcmp(s, "mmc1")) {
        spl_boot_list[0] = BOOT_DEVICE_MMC1;
#ifdef CONFIG_TARGET_HAILO15L_OREGANO
        spl_boot_list[1] = BOOT_DEVICE_MMC1;
#endif /* CONFIG_TARGET_HAILO15L_OREGANO */
    } else if (!strcmp(s, "mmc2")) {
        spl_boot_list[0] = BOOT_DEVICE_MMC2;
    } else if (!strcmp(s, "mmc12")) {
        spl_boot_list[0] = BOOT_DEVICE_MMC1;
        spl_boot_list[1] = BOOT_DEVICE_MMC2;
    } else if (!strcmp(s, "mmc21")) {
        spl_boot_list[0] = BOOT_DEVICE_MMC2;
        spl_boot_list[1] = BOOT_DEVICE_MMC1;
    } else if (!strcmp(s, "ram_mmc2")) {
        spl_boot_list[0] = BOOT_DEVICE_RAM;
        spl_boot_list[1] = BOOT_DEVICE_MMC2;
    } else if (!strcmp(s, "uart")) {
        spl_boot_list[0] = BOOT_DEVICE_UART;
    } else if (!strcmp(s, "ram")) {
        spl_boot_list[0] = BOOT_DEVICE_RAM;
    } else if (!strcmp(s, "nor")) {
        spl_boot_list[0] = BOOT_DEVICE_NOR;
    } else if (!strcmp(s, "mmc2_boot")) {
        spl_boot_list[0] = BOOT_DEVICE_MMC2;
        is_mmc2_boot = 1;
    }
     else {
        printf("spl_boot_source=%s unsupported, falling back to mmc12\n", s);
        s = "mmc12";
        spl_boot_list[0] = BOOT_DEVICE_MMC1;
        spl_boot_list[1] = BOOT_DEVICE_MMC2;
    }

    printf("U-Boot SPL boot source %s\n", s);
}

u32 spl_mmc_boot_mode(const u32 boot_device)
{
#if defined(CONFIG_SUPPORT_EMMC_BOOT)
    if(is_mmc2_boot && (boot_device == BOOT_DEVICE_MMC2)) 
    {
        return MMCSD_MODE_EMMCBOOT;
    }
#endif
#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
	return MMCSD_MODE_FS;
#elif defined(CONFIG_SUPPORT_EMMC_BOOT)
	return MMCSD_MODE_EMMCBOOT;
#else
	return MMCSD_MODE_RAW;
#endif
}

int spl_mmc_fs_boot_partition(void)
{
    return hailo15_mmc_boot_partition();
}

unsigned long spl_nor_get_uboot_base(void)
{
    return BASE_SPI_FLASH_ADDRESS + CONFIG_SYS_UBOOT_OFFSET + hailo15_get_active_boot_image_offset();
}

#ifdef CONFIG_SPL_LOAD_FIT

#if CONFIG_IS_ENABLED(OS_BOOT)
int spl_start_uboot(void)
{
	puts(SPL_TPL_PROMPT "Falcon mode: booting OS fitImage directly\n");
	return 0;  /* Boot kernel */
}
#endif

#ifdef CONFIG_MACH_HAILO10
/**
 * board_fit_config_name_match() - Check for a matching board name
 *
 * This is used when SPL loads a FIT containing multiple device tree files
 * and wants to work out which one to use. The description of each one is
 * passed to this function. The description comes from the 'description' field
 * in each (FDT) image node.
 *
 * @name: Device tree description
 * @return 0 if this device tree should be used, non-zero to try the next
 */
int board_fit_config_name_match(const char *name)
{
    char config_name[100];

    debug("Hailo10 board ID: %d\n", hailo10_sku_board_id);
    snprintf(config_name, sizeof(config_name), "board-sku-%d.dtb", hailo10_sku_board_id);
    debug("Hailo10 board ID name: %s\n", config_name);
    if (strstr(name, config_name)) {
        debug("Matched configuration: %s\n", config_name);
        return 0;
    }

    debug("No matching configuration found for %s\n", name);
    return -ENOENT;
}
#endif /* CONFIG_MACH_HAILO10 */
#endif /* CONFIG_SPL_LOAD_FIT */