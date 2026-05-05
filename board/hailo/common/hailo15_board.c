// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 */

#include <common.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>
#include <dm.h>
#include <dm/root.h>
#include <reset-uclass.h>
#include <scmi_base.h>
#include <hang.h>
#include <generated/autoconf.h>
#include <scmi_hailo.h>
#include <env.h>
#include <env_internal.h>
#include <linux/bitops.h>
#include <linux/sizes.h>
#include <dt-bindings/soc/hailo15_scu_fw_version.h>
#include <dt-bindings/soc/hailo15_ddr_config.h>
#include <spi.h>
#include <mmc.h>
#include <spi_flash.h>
#include <net.h>
#include <scmi_hailo_protocol.h>
#include <linux/libfdt.h>

#define MAC_ADDR_LEN 6
DECLARE_GLOBAL_DATA_PTR;

static struct udevice *scmi_agent_dev = NULL;
ulong active_boot_image_offset = 0;
ulong active_boot_image_storage = 0;

// Global variable to indicate if the boot image is in remote update mode, and need to set the corresponding env variable
uint8_t boot_image_mode = 0;
struct hailo15_dram_cfg {
    /* DDR ECC state */
    bool ecc_enable;
    /* DDR number of ranks */
    uint32_t num_of_ranks;
    /* DDR rank capacity */
    phys_size_t rank_capacity;
    /* bank total size */
    phys_size_t bank_total_size;
    /* bank usable size = (ecc_enable) ? (bank_total_size * 7 / 8) : bank_total_size*/
    phys_size_t bank_usable_size;
    /* number of enabled ddr elements - will be used to calculate the amount of memory bank_total_size*/
    uint8_t number_of_enabled_ddrs;
} hailo15_dram_cfg;

static enum env_location hailo_env_locations[] = {
#ifdef CONFIG_ENV_IS_IN_MMC
    ENVL_MMC,
#endif
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
    ENVL_SPI_FLASH,
#endif
#ifdef CONFIG_ENV_IS_IN_EEPROM
    ENVL_EEPROM,
#endif
#ifdef CONFIG_ENV_IS_IN_EXT4
    ENVL_EXT4,
#endif
#ifdef CONFIG_ENV_IS_IN_FAT
    ENVL_FAT,
#endif
#ifdef CONFIG_ENV_IS_IN_FLASH
    ENVL_FLASH,
#endif
#ifdef CONFIG_ENV_IS_IN_NAND
    ENVL_NAND,
#endif
#ifdef CONFIG_ENV_IS_IN_NVRAM
    ENVL_NVRAM,
#endif
#ifdef CONFIG_ENV_IS_IN_REMOTE
    ENVL_REMOTE,
#endif
#ifdef CONFIG_ENV_IS_IN_SATA
    ENVL_ESATA,
#endif
#ifdef CONFIG_ENV_IS_IN_UBI
    ENVL_UBI,
#endif
#ifdef CONFIG_ENV_IS_NOWHERE
    ENVL_NOWHERE,
#endif

};

enum env_location env_get_location(enum env_operation op, int prio)
{
    if (prio >= ARRAY_SIZE(hailo_env_locations))
        return ENVL_UNKNOWN;

    return hailo_env_locations[prio];
}

extern struct mm_region hailo15_mem_map[];
#if defined(CONFIG_MAC_ADDR_IN_SPIFLASH)
__weak int get_mac_addr_from_flash(u8 mac_addr[MAC_ADDR_LEN])
{
    struct spi_flash *flash;
    int ret;

    flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
                CONFIG_SF_DEFAULT_CS,
                CONFIG_SF_DEFAULT_SPEED,
                CONFIG_SF_DEFAULT_MODE);
    if (!flash) {
        printf("Error - unable to probe SPI flash.\n");
        return -1;
    }

    ret = spi_flash_read(flash, (CONFIG_MAC_ADDR_OFFSET), MAC_ADDR_LEN, mac_addr);
    if (ret) {
        printf("Error - unable to read MAC address from SPI flash.\n");
        return -1;
    }
    
    if (!is_valid_ethaddr(mac_addr)) {
        printf("Invalid MAC address from SPI flash.\n");
        return -1;
    }
        
    return 0;
}

__weak void set_mac_addr(void)
{
    uchar env_enetaddr[MAC_ADDR_LEN], mac_from_flash[MAC_ADDR_LEN];
    int enetaddr_found, spi_mac_read;

    enetaddr_found = eth_env_get_enetaddr("ethaddr", env_enetaddr);

    spi_mac_read = get_mac_addr_from_flash(mac_from_flash);

    /*
     * MAC address not present in the environment
     * try and read the MAC address from SPI flash
     * and set it.
     */
    if (!enetaddr_found) {
        if (!spi_mac_read) {
            if (eth_env_set_enetaddr("ethaddr", mac_from_flash)) {
                printf("Warning: Failed to "
                "set MAC address from SPI flash\n");
            }
        }
    } else {
        /*
         * MAC address present in environment compare it with
         * the MAC address in SPI flash and warn on mismatch
         */
        if (!spi_mac_read && memcmp(env_enetaddr, mac_from_flash, MAC_ADDR_LEN)){
            printf("Warning: MAC address in SPI flash don't match "
                    "with the MAC address in the environment\n");
            printf("Default using MAC address from environment\n");
        }
    }

}
#endif
ulong hailo15_get_active_boot_image_offset(void)
{
    return active_boot_image_offset;
}

int board_init(void)
{
    if (IS_ENABLED(CONFIG_SILENT_CONSOLE))
        gd->flags |= GD_FLG_SILENT;
    return 0;
}

int hailo15_scmi_check_version_match(void)
{
    u32 fw_version, impl_version;
    int ret;

    ret = dev_read_u32(scmi_agent_dev, "fw-ver", &fw_version);
    if (ret) {
        printf("Error reading fw-ver from SCMI devicetree node: ret=%d\n", ret);
        return ret;
    }

    ret = scmi_base_discover_implementation_version(scmi_agent_dev, &impl_version);
    if (ret) {
        printf("Error getting SCMI implmentation version: ret=%d\n", ret);
        return ret;
    }

    if (fw_version != impl_version) {
        printf("Firmware scmi version mismatch: u-boot(devicetree)=0x%x, fw=0x%x\n", fw_version, impl_version);
        return -EPERM;
    }
    if (SCU_FW_SCMI_VERSION != impl_version) {
        printf("Firmware scmi version mismatch: u-boot(binary)=0x%x, fw=0x%x\n", SCU_FW_SCMI_VERSION, impl_version);
        return -EPERM;
    }
    return 0;
}

int hailo15_scmi_init(void)
{
    int ret;
    struct scmi_hailo_get_boot_info_p2a boot_info;

    ret = uclass_first_device_err(UCLASS_SCMI_AGENT, &scmi_agent_dev);
    if (ret) {
        printf("Error retrieving SCMI agent uclass: ret=%d\n", ret);
        return ret;
    }

    ret = scmi_hailo_get_boot_info(scmi_agent_dev, &boot_info);
    if (ret) {
        printf("Error getting boot info via SCMI: ret=%d\n", ret);
        return ret;
    }

    active_boot_image_offset = boot_info.active_boot_image_offset;
    active_boot_image_storage = boot_info.active_boot_image_storage;

    // boot_image_mode is passed directly to bootmenu to test against
    boot_image_mode = boot_info.boot_image_mode;

#if defined (CONFIG_ENV_IS_IN_MMC) && defined (CONFIG_ENV_IS_IN_SPI_FLASH)
    /* if both configs are set, and boot_image_storage == flash, we change the priorities */
    if(BOOT_SOURCE_SPI_FLASH == active_boot_image_storage) {
        hailo_env_locations[0] = ENVL_SPI_FLASH;
        hailo_env_locations[1] = ENVL_MMC;
    }
#endif

    return 0;
}

int hailo15_send_scmi_boot_success(void)
{
    return scmi_hailo_send_boot_success_ind(scmi_agent_dev);
}

int board_early_init_r(void)
{
    /* initializing scmi must be early, before env is loaded,
        since the offset of the env in QSPI is dependent on it */
    return hailo15_scmi_init();
}

__weak int hailo15_mmc_boot_partition(void)
{
    if (active_boot_image_offset != 0) {
        return CONFIG_HAILO15_MMC_BOOT_PARTITION_B;
    }
    return CONFIG_HAILO15_MMC_BOOT_PARTITION;
}

__weak int hailo15_mmc_rootfs_partition(void)
{
    if (active_boot_image_offset != 0) {
        return CONFIG_HAILO15_MMC_ROOTFS_PARTITION_B;
    }
    return CONFIG_HAILO15_MMC_ROOTFS_PARTITION;
}

int misc_init_r(void)
{
    int ret = 0;
    env_set_hex("active_boot_image_offset", active_boot_image_offset);
    env_set_ulong("active_boot_image_storage", active_boot_image_storage);
    env_set_ulong("boot_image_mode", boot_image_mode);
    env_set_ulong("mmc_boot_partition", hailo15_mmc_boot_partition());
    env_set_ulong("mmc_rootfs_partition", hailo15_mmc_rootfs_partition());

    ret = scmi_hailo_send_boot_success_ind(scmi_agent_dev);
    if (ret) {
        printf("Error sending boot success indication via SCMI: ret=%d\n", ret);
        return ret;
    }

#if defined(CONFIG_MAC_ADDR_IN_SPIFLASH)
    set_mac_addr();
#endif
    /* checking for version match with the SCU, this is done here
        and not in board_early_init_r(), since in board_early_init_r() we don't yet have serial */
    return hailo15_scmi_check_version_match();
}

#define CS_MAP_ADDR 282
#define CS_MAP_OFFSET 16
#define CS_MAP_MASK GENMASK(17, 16)

#define CS_VAL_UPPER_0_ADDR 274
#define CS_VAL_UPPER_0_OFFSET 0
#define CS_VAL_UPPER_0_MASK GENMASK(15, 0)

#define DDR_CTRL_REGS_COUNT (0x19F)

int fdt_dram_cfg_get(void)
{
    char *ddr_cfg_path = "/hailo_boot_info/ddr_config";
    const fdt32_t *prop;
    uint32_t ecc_mode, cs_val_upper_0, cs_map;
    int len;
    int node;

    node = fdt_path_offset(gd->fdt_blob, ddr_cfg_path);
    if (node < 0) {
        printf("Error: fdt path (%s) doesn't exist\n", ddr_cfg_path);
        return -EINVAL;
    }

    /* Resolve ECC mode */
    prop = fdt_getprop(gd->fdt_blob, node, "ecc_mode", &len);
    if (prop == NULL) {
        printf("Error: fdt path (%s/ecc_mode) doesn't exist\n", ddr_cfg_path);
        return -EINVAL;
    }
    ecc_mode = fdt32_to_cpu(*prop);
    switch(ecc_mode) {
    case DDR_CTRL_ECC_MODE_DISABLED:
        hailo15_dram_cfg.ecc_enable = false;
        break;
    case DDR_CTRL_ECC_MODE_ENABLED:
    case DDR_CTRL_ECC_MODE_DETECTION:
    case DDR_CTRL_ECC_MODE_CORRECTION:
        hailo15_dram_cfg.ecc_enable = true;
        break;
    default:
        printf("Error: invalid ecc_mode value %x.\n", ecc_mode);
        return -EINVAL;
    }

    /* Read DDR controller regs */
    prop = fdt_getprop(gd->fdt_blob, node, "DDR_ctrl_registers", &len);
    if (prop == NULL) {
        printf("Error: fdt path (%s/DDR_ctrl_registers) doesn't exist.\n", ddr_cfg_path);
        return -EINVAL;
    }
    if (len != DDR_CTRL_REGS_COUNT * sizeof(uint32_t)) {
        printf("Error: fdt path (%s/DDR_ctrl_registers) invalid property length.\n", ddr_cfg_path);
        return -EINVAL;
        }
    cs_val_upper_0 = (fdt32_to_cpu(prop[CS_VAL_UPPER_0_ADDR]) & CS_VAL_UPPER_0_MASK) >> CS_VAL_UPPER_0_OFFSET;
    cs_map = (fdt32_to_cpu(prop[CS_MAP_ADDR]) & CS_MAP_MASK) >> CS_MAP_OFFSET;

    switch(cs_val_upper_0) {
    case 0x1FF:
        hailo15_dram_cfg.rank_capacity = SZ_256M; /* 256M Bytes */
        break;
    case 0x3FF:
        hailo15_dram_cfg.rank_capacity = SZ_512M; /* 512M Bytes */
        break;
    case 0x7FF:
        hailo15_dram_cfg.rank_capacity = SZ_1G; /* 1G Bytes */
        break;
    case 0xFFF:
        hailo15_dram_cfg.rank_capacity = SZ_2G; /* 2G Bytes */
        break;
    case 0x1FFF:
        hailo15_dram_cfg.rank_capacity = SZ_4G; /* 4G Bytes */
        break;
    default:
        printf("Error: invalid cs_val_upper_0 value %x.\n", cs_val_upper_0);
        return -EINVAL;
    }

    switch(cs_map) {
    case 0x1:
        hailo15_dram_cfg.num_of_ranks = 1;
        break;
    case 0x3:
        hailo15_dram_cfg.num_of_ranks = 2;
        break;
    default:
        printf("Error: invalid cs_map value %x.\n", cs_map);
        return -EINVAL;
    }

    /* Read enabled_ddrs */
    /* Setting the number_of_enabled_ddrs from the device tree - 1 ddr active if the property does not exist in case of Pluto/Mercury*/
    hailo15_dram_cfg.number_of_enabled_ddrs = 1;

    // Check existence and value of 'ddr_layout_mode' in device tree and setting ddr#_enabled accordingly
    prop = fdt_getprop(gd->fdt_blob, node, "ddr_layout_mode", &len);
    if (prop != NULL) {
        switch(fdt32_to_cpu(*prop)) {
        case DDR0_DDR1_LEGACY_LINEAR_MODE:
        case DDR0_DDR1_INTERLEAVING_512B_MODE:
        case DDR0_DDR1_INTERLEAVING_256B_MODE:
        case DDR0_DDR1_INTERLEAVING_4KB_MODE:
            hailo15_dram_cfg.number_of_enabled_ddrs = 2;
            break;
        case ONLY_DDR0_ENABLED_MODE:
            hailo15_dram_cfg.number_of_enabled_ddrs = 1;
            break;
        case ONLY_DDR1_ENABLED_MODE:
            hailo15_dram_cfg.number_of_enabled_ddrs = 1;
            break;
        default:
            printf("Error: invalid ddr_layout_mode value %x.\n", fdt32_to_cpu(*prop));
            return -EINVAL;
        }
    }else{
        // If ddr_layout_mode property does not exist(PLT or MERC), we set it as DDR0_ENABLED
        hailo15_dram_cfg.number_of_enabled_ddrs = 1;
    }

    hailo15_dram_cfg.bank_total_size = hailo15_dram_cfg.rank_capacity * hailo15_dram_cfg.num_of_ranks;
    hailo15_dram_cfg.bank_usable_size = hailo15_dram_cfg.bank_total_size;
    if (hailo15_dram_cfg.ecc_enable) {
        hailo15_dram_cfg.bank_usable_size = hailo15_dram_cfg.bank_total_size * 7ULL / 8ULL;
    }
    hailo15_dram_cfg.bank_usable_size *= hailo15_dram_cfg.number_of_enabled_ddrs;

    return 0;
}

/* In SPL we don't use dram_init() */
#ifndef CONFIG_SPL_BUILD

int dram_init(void)
{
    int ret;
    ret = fdt_dram_cfg_get();
    if (ret) {
        return ret;
    }

    hailo15_mem_map[0].size = hailo15_dram_cfg.bank_usable_size;

    gd->ram_size = hailo15_mem_map[0].size;

    return 0;
}

#endif /* !CONFIG_SPL_BUILD */

/*! @note Need to add back the appropriate DDR reg configs to veloce/ginger/... also */
int dram_init_banksize(void)
{
    int ret;

    ret = fdt_dram_cfg_get();
    if (ret) {
        return ret;
    }

    gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
    gd->bd->bi_dram[0].size = hailo15_dram_cfg.bank_usable_size;

    return 0;
}

#if defined(CONFIG_SHOW_BOOT_PROGRESS)
void show_boot_progress(int progress)
{
    printf("Boot reached stage %d\n", progress);
}
#endif

/*
 * return configured FDT blob address
 */
void *board_fdt_blob_setup(int *err)
{
    unsigned long fw_dtb = CONFIG_HAILO15_DTB_ADDRESS;

    log_debug("%s: fw_dtb=%lx\n", __func__, fw_dtb);

    *err = 0;

    return (void *)fw_dtb;
}

ulong env_sf_get_env_offset(void)
{
    return ((ulong)CONFIG_ENV_OFFSET) + active_boot_image_offset;
}

int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	*env_addr = ((ulong)CONFIG_ENV_OFFSET) + active_boot_image_offset;

    if(copy > 0)
    {
        pr_warn("mmc_get_env_addr: copy is > 0 (=%d), ignoring\n", copy);
    }
    
	return 0;
}

static void fixup_linux_cma_size(void *fdt, uint64_t new_size)
{
    const char *node_path = "/reserved-memory/linux,cma";
    int node, reg_len, ret;
    const void *orig_prop;
    fdt64_t reg[2];

    /* Locate the linux,cma reserved-memory node */
    node = fdt_path_offset(fdt, node_path);
    if (node < 0) {
        printf("Warning: %s node not found: %s\n",
                node_path, fdt_strerror(node));
        return;
    }

    /* Read the existing "reg" property */
    orig_prop = fdt_getprop(fdt, node, "reg", &reg_len);
    if (!orig_prop || reg_len != sizeof(reg)) {
        printf("Warning: failed to read reg for %s: %s\n",
                node_path, fdt_strerror(reg_len));
        return;
    }
    memcpy(reg, orig_prop, sizeof(reg));

    /* Override the size (second cell) */
    reg[1] = cpu_to_fdt64(new_size);

    /* Write the updated "reg" back into the DT */
    ret = fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
    if (ret) {
        printf("Warning: failed to override reg for %s: %s\n",
                node_path, fdt_strerror(ret));
        return;
    }
}

static void disable_hailo_cma(void *fdt)
{
    const char *node_path = "/reserved-memory/hailo_media_buf,cma";
    int node, ret;

    node = fdt_path_offset(fdt, node_path);
    if (node < 0) {
        printf("Warning: %s node not found: %s\n", node_path, fdt_strerror(node));
        return;
    }

    ret = fdt_setprop_string(fdt, node, "status", "disabled");
    if (ret) {
        printf("Warning: failed to disable %s node: %s\n", node_path, fdt_strerror(ret));
        return;
    }
}

void board_fixup_fdt_reserved_mem(void *fdt)
{
    if (env_get_yesno("shrink_cma") == 1) {
        fixup_linux_cma_size(fdt, 32UL * SZ_1M);
        disable_hailo_cma(fdt);
    }
}
