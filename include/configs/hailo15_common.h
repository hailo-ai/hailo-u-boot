/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.  
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO15_COMMON_H
#define __HAILO15_COMMON_H

/* Physical Memory Map */
#define PHYS_SDRAM_1 (0x80000000) /* SDRAM Bank #1 */

/* additions for new relocation code */
#define CONFIG_SYS_SDRAM_BASE (PHYS_SDRAM_1)
#define CONFIG_SYS_INIT_SP_ADDR (0x83000000)

/* GICv3 */
#define GICD_BASE (0x60600000)
#define GICR_BASE (GICD_BASE + 0x80000)

#define CPU_RELEASE_ADDR (0x800FF000)

#define CONFIG_SYS_MAX_FLASH_BANKS (1)
#define COUNTER_FREQUENCY (25000000) // based on xtal clock - 25Mhz
#define CONFIG_SYS_BOOTM_LEN (0x8000000)

#define CONFIG_SPL_BSS_START_ADDR	0x82000000
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot-tfa.itb"

#ifdef CONFIG_HAILO15_SWUPDATE

#ifndef SWUPDATE_MMC_INDEX
#define SWUPDATE_MMC_INDEX "0"
#endif /* SWUPDATE_MMC_INDEX */

#define SWUPDATE_EXTRA_ENV_SETTINGS \
    "swupdate_ram_addr=0xB0000000\0" \
    "load_swupdate_image_from_mmc=" UNNEEDED_MMCINFO_HACK " fatload mmc ${device_num} ${swupdate_ram_addr} swupdate-image-" CONFIG_SYS_BOARD ".ext4.gz && setenv swupdate_filesize ${filesize}\0" \
    "write_swupdate_image_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num} ${swupdate_ram_addr} swupdate-image-" CONFIG_SYS_BOARD ".ext4.gz ${filesize}\0" \
    "download_swupdate_image_to_ram=run set_eth_env && tftpboot ${swupdate_ram_addr} swupdate-image-" CONFIG_SYS_BOARD ".ext4.gz\0" \
    "swupdate_server_udp_logging_port=12345\0" \
    "swupdate_update_filename=hailo-update-image-" CONFIG_SYS_BOARD ".swu\0" \
    "set_swupdate_bootargs=run set_eth_env && setenv bootargs ${bootargs_base} root=/dev/ram0 ramdisk_size=1000000 SWUPDATE_SERVER_IP=${serverip} SWUPDATE_SERVER_UDP_LOGGING_PORT=${swupdate_server_udp_logging_port} SWUPDATE_UPDATE_FILENAME=${swupdate_update_filename}\0" \
    "boot_swupdate=run set_mmc" SWUPDATE_MMC_INDEX "_device_num && run load_fitimage_from_mmc && run load_swupdate_image_from_mmc && run set_swupdate_bootargs && bootm ${far_ram_addr} ${swupdate_ram_addr}:${swupdate_filesize}\0" \
    "update_swupdate_image=run download_swupdate_image_to_ram && run write_swupdate_image_to_mmc\0" \
    "boot_swupdate_ab=setenv swupdate_update_filename hailo-ab-update-image-" CONFIG_SYS_BOARD ".swu && run boot_swupdate\0"

#define UPDATE_PARTITIONS_COMMAND "update_partitions=run update_uboot && run update_fitimage && run update_swupdate_image && run update_rootfs\0"

#ifndef SWUPDATE_BOOTMENU_OPTION
#define SWUPDATE_BOOTMENU_OPTION "bootmenu_8=SWUpdate=run boot_swupdate\0"
#endif /* SWUPDATE_BOOTMENU_OPTION */

#else

#define SWUPDATE_EXTRA_ENV_SETTINGS ""
#define SWUPDATE_BOOTMENU_OPTION ""
#define UPDATE_PARTITIONS_COMMAND "update_partitions=run update_uboot && run update_fitimage && run update_rootfs\0"

#endif /* CONFIG_HAILO15_SWUPDATE */

#ifndef BOOTMENU
#ifndef CONFIG_HAILO15_EMMC_8BIT
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from SD; run boot_mmc0;" \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from SD Card=run boot_mmc0\0" \
    "bootmenu_2=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_3=Update SD (wic) from TFTP=run update_wic_mmc0 && bootmenu -1\0" \
    "bootmenu_4=Update eMMC (wic) from TFTP=run update_wic_mmc1 && bootmenu -1\0" \
    "bootmenu_5=Update SD (partitions) from TFTP=run update_partitions_mmc0 && bootmenu -1\0" \
    "bootmenu_6=Update eMMC (partitions) from TFTP=run update_partitions_mmc1 && bootmenu -1\0" \
    "bootmenu_7=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=mmc12\0" \
    "spl_boot_source=mmc12\0"
#else
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_2=Update eMMC (wic) from TFTP=run update_wic_mmc1 && bootmenu -1\0" \
    "bootmenu_3=Update eMMC (partitions) from TFTP=run update_partitions_mmc1 && bootmenu -1\0" \
    "bootmenu_4=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=mmc2\0" \
    "spl_boot_source=mmc2\0"
#endif

#endif

/* extra build is only relevant in full u-boot */
#ifndef CONFIG_SPL_BUILD

/* used this for tftp to mmc: https://stackoverflow.com/a/47594311 */
/* see MSW-714 for details */
#define UNNEEDED_MMCINFO_HACK "mmcinfo;"

#define CONFIG_EXTRA_ENV_SETTINGS \
    "bootargs_base=console=ttyS1,115200n8 earlycon loglevel=8 rootwait debug rw\0" \
    "far_ram_addr=0x85000000\0" \
    "get_rootfs_partition_start_offset=part start mmc ${device_num} 2 rootfs_partition_start_offset\0" \
    "sd_block_size=200\0" /* in hex, taken from running mmcinfo */\
    "set_eth_env=setenv serverip 10.0.0.2; setenv ipaddr 10.0.0.1\0" \
    "set_mmc0_device_num= setenv device_num 0 && mmc dev ${device_num}\0" \
    "set_mmc1_device_num= setenv device_num 1 && mmc dev ${device_num}\0" \
    "load_fitimage_from_mmc=" UNNEEDED_MMCINFO_HACK " fatload mmc ${device_num} ${far_ram_addr} fitImage\0" \
    "write_fitimage_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num} ${far_ram_addr} fitImage ${filesize}\0" \
    "write_uboot_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num} ${far_ram_addr} " CONFIG_SPL_FS_LOAD_PAYLOAD_NAME " ${filesize}\0" \
    "write_uboot_to_mmc0_mmc1=run set_mmc0_device_num && run write_uboot_to_mmc; run set_mmc1_device_num && run write_uboot_to_mmc\0" \
    /* "mmc write" writes in blocks, so we first calculate the number of blocks we read into wic_sdblock_count. */\
    /* we assume this is called after 'tftpboot' - so filesize is populated */\
    "write_wic_to_mmc=setexpr wic_sdblock_count ${filesize} / ${sd_block_size} && setexpr wic_sdblock_count ${wic_sdblock_count} + 1; " UNNEEDED_MMCINFO_HACK " mmc write ${far_ram_addr} 0 ${wic_sdblock_count}\0" \
    "write_rootfs_to_mmc=setexpr rootfs_sdblock_count ${filesize} / ${sd_block_size} && setexpr rootfs_sdblock_count ${rootfs_sdblock_count} + 1; " UNNEEDED_MMCINFO_HACK " run get_rootfs_partition_start_offset && mmc write ${far_ram_addr} ${rootfs_partition_start_offset} ${rootfs_sdblock_count}\0" \
    /* tftpboot sets filesize to the size it loaded */\
    "download_wic_to_ram=run set_eth_env && tftpboot ${far_ram_addr} core-image-minimal-" CONFIG_SYS_BOARD ".wic\0" \
    "download_rootfs_to_ram=run set_eth_env && tftpboot ${far_ram_addr} core-image-minimal-" CONFIG_SYS_BOARD ".ext4\0" \
    "download_fitimage_to_ram=run set_eth_env && tftpboot ${far_ram_addr} fitImage\0" \
    "download_uboot_to_ram=run set_eth_env && tftpboot ${far_ram_addr} " CONFIG_SPL_FS_LOAD_PAYLOAD_NAME "\0" \
    "boot_mmc= setenv bootargs ${bootargs_base} root=/dev/mmcblk${device_num}p2; run load_fitimage_from_mmc && bootm ${far_ram_addr}\0" \
    "boot_mmc0=run set_mmc0_device_num && run boot_mmc\0"\
    "boot_mmc1=run set_mmc1_device_num && run boot_mmc\0"\
    "update_wic=run download_wic_to_ram && run write_wic_to_mmc\0" \
    "update_wic_mmc0=run set_mmc0_device_num && run update_wic\0" \
    "update_wic_mmc1=run set_mmc1_device_num && run update_wic\0" \
    "update_rootfs=run download_rootfs_to_ram && run write_rootfs_to_mmc\0" \
    "update_fitimage=run download_fitimage_to_ram && run write_fitimage_to_mmc\0" \
    "update_uboot=run download_uboot_to_ram && run write_uboot_to_mmc\0" \
    "update_uboot_mmc0_mmc1=run download_uboot_to_ram && run write_uboot_to_mmc0_mmc1\0" \
    "update_partitions_mmc0=run set_mmc0_device_num && run update_partitions\0"\
    "update_partitions_mmc1=run set_mmc1_device_num && run update_partitions\0"\
    "nfsroot=/mnt/hailo15_nfs/\0"\
    "bootargs_nfs=setenv bootargs ${bootargs_base} root=/dev/nfs rootfstype=nfs ip=${ipaddr} nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
    "bootnfs=run download_fitimage_to_ram && run bootargs_nfs && bootm ${far_ram_addr}\0" \
    "auto_uboot_update=run update_uboot_mmc0_mmc1; env set auto_uboot_update_enable no && env set spl_boot_source ${default_spl_boot_source} && env set bootdelay ${default_bootdelay} && saveenv\0" \
    "auto_uboot_update_enable=no\0" \
    "default_bootdelay=2\0" \
    BOOTMENU \
    UPDATE_PARTITIONS_COMMAND \
    SWUPDATE_EXTRA_ENV_SETTINGS \
    SWUPDATE_BOOTMENU_OPTION

#endif /* !CONFIG_SPL_BUILD */

#endif /* __HAILO15_COMMON_H */

