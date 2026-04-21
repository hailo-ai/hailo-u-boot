/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * Configuration for Hailo15.
 */

#ifndef __HAILO15_FAMILY_COMMON_H
#define __HAILO15_FAMILY_COMMON_H

#define CONFIG_SYS_MAXARGS 128

/* Physical Memory Map */
#define PHYS_SDRAM_1 (0x80000000) /* SDRAM Bank #1 */

/* additions for new relocation code */
#define CONFIG_SYS_SDRAM_BASE (PHYS_SDRAM_1)
#define CONFIG_SYS_INIT_SP_ADDR (0x80700000)

/* GICv3 */
#define GICD_BASE (0x60600000)
#define GICR_BASE (GICD_BASE + 0x80000)

#define CPU_RELEASE_ADDR (0x800FF000)

#define CONFIG_SYS_MAX_FLASH_BANKS (1)
#define COUNTER_FREQUENCY (25000000) // based on xtal clock - 25Mhz
#define CONFIG_SYS_BOOTM_LEN (0x8000000)  /* 128MB for kernel decompression */

#define CONFIG_SPL_MAX_SIZE (0x0002c000)
#define CONFIG_SPL_BSS_START_ADDR (0x8010f000)
#define CONFIG_SPL_STACK (0x801D4000)
#define CONFIG_SPL_BSS_MAX_SIZE (0x2000)
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME "u-boot-tfa.itb"
// Dummy value
#define CONFIG_SYS_UBOOT_BASE 0

#ifndef BOOTARGS_BASE
#define BOOTARGS_BASE "console=ttyS1,${baudrate}n8 earlycon loglevel=8 rootwait debug"
#endif
#ifndef DEFAULT_BOOTDELAY
#define DEFAULT_BOOTDELAY "2"
#endif

#ifdef CONFIG_HAILO15_SWUPDATE

#ifndef SWUPDATE_MMC_INDEX
#define SWUPDATE_MMC_INDEX "0"
#endif /* SWUPDATE_MMC_INDEX */

#define SWUPDATE_EXTRA_ENV_SETTINGS \
    "load_swupdate_image_from_mmc=" UNNEEDED_MMCINFO_HACK " fatload mmc ${device_num}:${mmc_boot_partition} ${fs_ram_addr} swupdate-image-${board}.ext4.gz && setenv swupdate_filesize ${filesize}\0" \
    "write_swupdate_image_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num}:${mmc_boot_partition} ${fs_ram_addr} swupdate-image-${board}.ext4.gz ${filesize}\0" \
    "download_swupdate_image_to_ram= tftpboot ${fs_ram_addr} swupdate-image-${board}.ext4.gz && setenv swupdate_filesize ${filesize}\0" \
    "swupdate_server_udp_logging_port=12345\0" \
    "setup_swupdate_update_filename=setenv swupdate_update_filename hailo-update-image-${board}.swu\0" \
    "setup_swupdate_server_ip=if test -z \"${swupdate_server_ip}\"; then setenv swupdate_server_ip ${serverip}; fi; true\0" \
    "swupdate_update_modes=init-partitions-single,init-scu-bl,copy-a\0" \
    "append_bootargs_swupdate_server=setenv bootargs ${bootargs} SWUPDATE_SERVER_IP=${swupdate_server_ip} SWUPDATE_IPADDR=${ipaddr}\0" \
    "append_bootargs_swupdate_params=setenv bootargs ${bootargs} SWUPDATE_SERVER_UDP_LOGGING_PORT=${swupdate_server_udp_logging_port} SWUPDATE_UPDATE_FILENAME=${swupdate_update_filename} SWUPDATE_UPDATE_MODES=${swupdate_update_modes}\0" \
    "append_bootargs_swupdate_device=setenv bootargs ${bootargs} ${bootargs_swupdate_device}\0" \
    "bootargs_swupdate=run setup_swupdate_server_ip && run setup_swupdate_update_filename && run append_bootargs_swupdate_server && run append_bootargs_swupdate_params && run append_bootargs_swupdate_device\0" \
    "swupdate_load_mmc=run set_mmc" SWUPDATE_MMC_INDEX "_device_num && run load_fitimage_from_mmc && run load_swupdate_image_from_mmc\0" \
    "swupdate_load_tftp=run download_fitimage_to_ram && run download_swupdate_image_to_ram\0" \
    "boot_swupdate=run bootargs_base bootargs_rw bootargs_nand_fitimage bootargs_swupdate && setenv shrink_cma 1 && setenv boot_ram_size ${swupdate_filesize} && run _do_boot_ram\0" \
    "setup_swupdate_sdio0_filesystem_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_FILESYSTEM_DEVICE=mmcblk0\0" \
    "setup_swupdate_sdio1_filesystem_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_FILESYSTEM_DEVICE=mmcblk1\0" \
    "setup_swupdate_flash_firmware_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_FIRMWARE_DEVICE=mtdblock0 SWUPDATE_FW_ENV_DEVICE=mtdblock0\0" \
    "setup_swupdate_mmc0_firmware_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_FIRMWARE_DEVICE=mmcblk0boot0 SWUPDATE_FW_ENV_DEVICE=mmcblk0boot0\0" \
    "setup_swupdate_mmc1_firmware_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_FIRMWARE_DEVICE=mmcblk1boot0 SWUPDATE_FW_ENV_DEVICE=mmcblk1boot0\0" \
    "setup_swupdate_firmware_device=if test ${active_boot_image_storage} = " __stringify(BOOT_SOURCE_SPI_FLASH) "; then run setup_swupdate_flash_firmware_device;fi;" \
                                   "if test ${active_boot_image_storage} = " __stringify(BOOT_SOURCE_EMMC0) "; then run setup_swupdate_mmc0_firmware_device;fi;" \
                                   "if test ${active_boot_image_storage} = " __stringify(BOOT_SOURCE_EMMC1) "; then run setup_swupdate_mmc1_firmware_device;fi;" \
                                   "exit 0;\0" /* ensure script does not error */ \
    "setup_swupdate_nand_flash_filesystem_device=setenv bootargs_swupdate_device ${bootargs_swupdate_device} SWUPDATE_BOOTLOADER_DEVICE=mtdblock0 SWUPDATE_FITIMAGE_DEVICE=mtd2 SWUPDATE_ROOTFS_DEVICE=mtd3\0" \
    "boot_swupdate_mmc=run swupdate_load_mmc && run setup_swupdate_firmware_device && run boot_swupdate\0" \
    "update_swupdate_image=run download_swupdate_image_to_ram && run write_swupdate_image_to_mmc\0" \
    "boot_swupdate_sdio0_only_a=run setup_swupdate_sdio0_filesystem_device && setenv swupdate_update_modes init-partitions-single,init-scu-bl,copy-a && run swupdate_load_tftp && run boot_swupdate\0" \
    "boot_swupdate_sdio0_ab=run setup_swupdate_sdio0_filesystem_device && setenv swupdate_update_modes init-partitions-dual,init-scu-bl,copy-a,copy-b && run swupdate_load_tftp && run boot_swupdate\0" \
    "boot_swupdate_sdio1_only_a=run setup_swupdate_sdio1_filesystem_device && setenv swupdate_update_modes init-partitions-single,init-scu-bl,copy-a && run swupdate_load_tftp && run boot_swupdate\0" \
    "boot_swupdate_sdio1_ab=run setup_swupdate_sdio1_filesystem_device && setenv swupdate_update_modes init-partitions-dual,init-scu-bl,copy-a,copy-b && run swupdate_load_tftp && run boot_swupdate\0" \
    "boot_swupdate_nand_flash_only_a=run setup_swupdate_flash_firmware_device && run setup_swupdate_nand_flash_filesystem_device && setenv swupdate_update_modes init-partitions-single,copy-a && run swupdate_load_tftp && run boot_swupdate\0"

#define UPDATE_PARTITIONS_COMMAND "update_partitions=run update_uboot && run update_fitimage && run update_swupdate_image && run update_rootfs\0"

#ifndef SWUPDATE_BOOTMENU_OPTION
#if !defined(CONFIG_HAILO15_EMMC_8BIT) && !defined(CONFIG_HAILO_NAND_BOOT)
#define SWUPDATE_BOOTMENU_OPTION "bootmenu_4=SD Card Board Init=run boot_swupdate_sdio0_only_a\0" \
                                 "bootmenu_5=SD Card AB Board Init=run boot_swupdate_sdio0_ab\0" \
                                 "bootmenu_6=eMMC Board Init=run boot_swupdate_sdio1_only_a\0" \
                                 "bootmenu_7=eMMC AB Board Init=run boot_swupdate_sdio1_ab\0"
#elif defined(CONFIG_HAILO15_EMMC_8BIT) && !defined(CONFIG_HAILO_NAND_BOOT)
#define SWUPDATE_BOOTMENU_OPTION "bootmenu_3=eMMC Board Init=run boot_swupdate_sdio1_only_a\0" \
                                 "bootmenu_4=eMMC AB Board Init=run boot_swupdate_sdio1_ab\0"
#elif !defined(CONFIG_HAILO15_EMMC_8BIT) && defined(CONFIG_HAILO_NAND_BOOT)
#define SWUPDATE_BOOTMENU_OPTION "bootmenu_5=NAND Flash Board Init=run boot_swupdate_nand_flash_only_a\0" \
                                 "bootmenu_6=SD Card Board Init=run boot_swupdate_sdio0_only_a\0" \
                                 "bootmenu_7=SD Card AB Board Init=run boot_swupdate_sdio0_ab\0" \
                                 "bootmenu_8=eMMC Board Init=run boot_swupdate_sdio1_only_a\0" \
                                 "bootmenu_9=eMMC AB Board Init=run boot_swupdate_sdio1_ab\0"
#else /* CONFIG_HAILO15_EMMC_8BIT && CONFIG_HAILO_NAND_BOOT */
#define SWUPDATE_BOOTMENU_OPTION "bootmenu_4=NAND Flash Board Init=run boot_swupdate_nand_flash_only_a\0" \
                                 "bootmenu_5=eMMC Board Init=run boot_swupdate_sdio1_only_a\0" \
                                 "bootmenu_6=eMMC AB Board Init=run boot_swupdate_sdio1_ab\0"
#endif /* CONFIG_HAILO15_EMMC_8BIT && CONFIG_HAILO_NAND_BOOT */
#endif /* SWUPDATE_BOOTMENU_OPTION */
#else

#define SWUPDATE_EXTRA_ENV_SETTINGS ""
#define SWUPDATE_BOOTMENU_OPTION ""
#define UPDATE_PARTITIONS_COMMAND "update_partitions=run update_uboot && run update_fitimage && run update_rootfs\0"

#endif /* CONFIG_HAILO15_SWUPDATE */

#ifndef SPL_BOOT_SOURCE
#define SPL_BOOT_SOURCE "mmc21"
#endif

#ifndef BOOTMENU
#if !defined(CONFIG_HAILO15_EMMC_8BIT) && !defined(CONFIG_HAILO_NAND_BOOT)
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test ${boot_image_mode} = 1; then run boot_swupdate_mmc; exit 1; fi; " \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from SD; run boot_mmc0;" \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from SD Card=run boot_mmc0\0" \
    "bootmenu_2=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_3=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "spl_boot_source=" SPL_BOOT_SOURCE "\0"
#elif defined(CONFIG_HAILO15_EMMC_8BIT) && !defined(CONFIG_HAILO_NAND_BOOT)
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test ${boot_image_mode} = 1; then run boot_swupdate_mmc; exit 1; fi; " \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_2=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=mmc2\0" \
    "spl_boot_source=mmc2\0"
#elif !defined(CONFIG_HAILO15_EMMC_8BIT) && defined(CONFIG_HAILO_NAND_BOOT)
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test ${boot_image_mode} = 1; then run boot_swupdate_mmc; exit 1; fi; " \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from NAND Flash; run boot_nand_flash;" \
        "echo Trying Boot from SD; run boot_mmc0;" \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from NAND Flash=run boot_nand_flash\0" \
    "bootmenu_2=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_3=Boot from SD Card=run boot_mmc0\0" \
    "bootmenu_4=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=" SPL_BOOT_SOURCE "\0" \
    "spl_boot_source=" SPL_BOOT_SOURCE "\0"
#else /* CONFIG_HAILO15_EMMC_8BIT && CONFIG_HAILO_NAND_BOOT */
#define BOOTMENU \
    /* Try all boot options by order */ \
    "bootmenu_0=Autodetect=" \
        "if test ${boot_image_mode} = 1; then run boot_swupdate_mmc; exit 1; fi; " \
        "if test \"${auto_uboot_update_enable}\" = \"yes\"; then run auto_uboot_update; exit 1; fi; " \
        "echo Trying Boot from NAND Flash; run boot_nand_flash;" \
        "echo Trying Boot from eMMC; run boot_mmc1;" \
        "echo Trying Boot from NFS; run bootnfs;" \
        "echo ERROR: All boot options failed\0" \
    "bootmenu_1=Boot from NAND Flash=run boot_nand_flash\0" \
    "bootmenu_2=Boot from eMMC=run boot_mmc1\0" \
    "bootmenu_3=Boot from NFS=run bootnfs\0" \
    "default_spl_boot_source=mmc2\0" \
    "spl_boot_source=mmc2\0"
#endif /* CONFIG_HAILO15_EMMC_8BIT && CONFIG_HAILO_NAND_BOOT */
#endif /* BOOTMENU */

#ifndef BOOT_COMMAND
#define BOOT_COMMAND "bootm ${fitimage_ram_addr}#conf-${vendor}_${board_dtb}.dtb${dtb_overlays} ${initrd_arg}"
#endif

/* extra build is only relevant in full u-boot */
#ifndef CONFIG_SPL_BUILD

/* used this for tftp to mmc: https://stackoverflow.com/a/47594311 */
/* see MSW-714 for details */
#define UNNEEDED_MMCINFO_HACK "mmcinfo;"

#define CONFIG_EXTRA_ENV_SETTINGS \
    "board_dtb=" CONFIG_SYS_BOARD "\0" \
    "initrd_arg=-\0" \
    "bootargs_base=setenv bootargs " BOOTARGS_BASE " && run bootargs_board\0" \
    "bootargs_rw=setenv bootargs ${bootargs} rw\0" \
    "bootargs_ro=setenv bootargs ${bootargs} ro\0" \
    "bootargs_board=setenv bootargs ${bootargs} ${bootargs_board_options}\0" \
    "bootargs_ram=setenv bootargs ${bootargs} root=/dev/ram0 ramdisk_size=${ramdisk_size}\0" \
    "bootargs_nand_fitimage=setenv bootargs ${bootargs} ubi.mtd=2,0,5\0" \
    "bootargs_mmc=setenv bootargs ${bootargs} root=/dev/mmcblk${device_num}p${mmc_rootfs_partition}\0" \
    "bootargs_nand_flash=setenv bootargs ${bootargs} ubi.mtd=3 root=ubi0:rootfs rootfstype=ubifs\0" \
    "bootargs_nfs=setenv bootargs ${bootargs} root=/dev/nfs rootfstype=nfs ip=${ipaddr} nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
    "ramdisk_size=0x80000\0" \
    "fitimage_ram_addr=0x88000000\0" \
    "fs_ram_addr=0x90000000\0" \
    "get_rootfs_partition_start_offset=part start mmc ${device_num} ${mmc_rootfs_partition} rootfs_partition_start_offset\0" \
    "sd_block_size=200\0" /* in hex, taken from running mmcinfo */\
    "serverip=10.0.0.2\0" \
    "ipaddr=10.0.0.1\0" \
    "initrd_high=0xffffffffffffffff\0" /* disables relocation of initrd/initramfs - this saves a significant amount of boot time */ \
    "core_image_name=" CONFIG_CORE_IMAGE_NAME "\0" \
    "set_mmc0_device_num= setenv device_num 0 && mmc dev ${device_num}\0" \
    "set_mmc1_device_num= setenv device_num 1 && mmc dev ${device_num}\0" \
    "load_fitimage_from_mmc=" UNNEEDED_MMCINFO_HACK " fatload mmc ${device_num}:${mmc_boot_partition} ${fitimage_ram_addr} fitImage\0" \
    "load_fitimage_from_nand_flash=setenv mtdparts spi-nand0:2m(bootloader),18m(fitImage),492m(rootfs) && mtdparts; ubi part fitImage && ubi read ${fitimage_ram_addr} fitImage 0x1200000\0" \
    "write_fitimage_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num}:${mmc_boot_partition} ${fitimage_ram_addr} fitImage ${filesize}\0" \
    "write_uboot_to_mmc=" UNNEEDED_MMCINFO_HACK " fatwrite mmc ${device_num}:${mmc_boot_partition} ${fitimage_ram_addr} " CONFIG_SPL_FS_LOAD_PAYLOAD_NAME " ${filesize}\0" \
    "write_uboot_to_mmc0_mmc1=run set_mmc0_device_num && run write_uboot_to_mmc; run set_mmc1_device_num && run write_uboot_to_mmc\0" \
    /* "mmc write" writes in blocks, so we first calculate the number of blocks we read into wic_sdblock_count. */\
    /* we assume this is called after 'tftpboot' - so filesize is populated */\
    "write_wic_to_mmc=setexpr wic_sdblock_count ${filesize} / ${sd_block_size} && setexpr wic_sdblock_count ${wic_sdblock_count} + 1; " UNNEEDED_MMCINFO_HACK " mmc write ${fs_ram_addr} 0 ${wic_sdblock_count}\0" \
    "write_rootfs_to_mmc=setexpr rootfs_sdblock_count ${filesize} / ${sd_block_size} && setexpr rootfs_sdblock_count ${rootfs_sdblock_count} + 1; " UNNEEDED_MMCINFO_HACK " run get_rootfs_partition_start_offset && mmc write ${fs_ram_addr} ${rootfs_partition_start_offset} ${rootfs_sdblock_count}\0" \
    /* tftpboot sets filesize to the size it loaded */\
    "download_wic_to_ram=tftpboot ${fs_ram_addr} ${core_image_name}-${board}.wic\0" \
    "download_rootfs_to_ram=tftpboot ${fs_ram_addr} ${core_image_name}-${board}.ext4\0" \
    "download_fitimage_to_ram=tftpboot ${fitimage_ram_addr} fitImage\0" \
    "dtb_overlays= \0" /* added space otherwise it gets removed */ \
    "download_uboot_to_ram=tftpboot ${fitimage_ram_addr} " CONFIG_SPL_FS_LOAD_PAYLOAD_NAME "\0" \
    "boot=" BOOT_COMMAND "\0" \
    "_do_boot_ram=run bootargs_ram && setenv initrd_arg ${fs_ram_addr}:${boot_ram_size} && run boot\0" \
    "boot_ram=run bootargs_base && run _do_boot_ram\0" \
    "boot_mmc=run bootargs_base bootargs_rw bootargs_mmc && run load_fitimage_from_mmc && run boot\0" \
    "boot_nand_flash=run bootargs_base bootargs_rw bootargs_nand_flash && run load_fitimage_from_nand_flash && run boot\0"\
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
    "bootnfs=run bootargs_base bootargs_rw bootargs_nfs && run download_fitimage_to_ram && run boot\0" \
    "auto_uboot_update=run update_uboot_mmc0_mmc1; env set auto_uboot_update_enable no && env set spl_boot_source ${default_spl_boot_source} && env set bootdelay ${default_bootdelay} && saveenv\0" \
    "auto_uboot_update_enable=no\0" \
    "default_bootdelay=" DEFAULT_BOOTDELAY "\0" \
    BOOTMENU \
    UPDATE_PARTITIONS_COMMAND \
    SWUPDATE_EXTRA_ENV_SETTINGS \
    SWUPDATE_BOOTMENU_OPTION

#endif /* !CONFIG_SPL_BUILD */

#endif /* __HAILO15_FAMILY_COMMON_H */

