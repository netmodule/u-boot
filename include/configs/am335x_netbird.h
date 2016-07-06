/*
 * am335x_evm.h
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __CONFIG_AM335X_EVM_H
#define __CONFIG_AM335X_EVM_H

#include <configs/ti_am335x_common.h>

#undef CONFIG_SPL_AM33XX_ENABLE_RTC32K_OSC
#undef CONFIG_HW_WATCHDOG
#undef CONFIG_OMPAP_WATCHDOG
#undef CONFIG_SPL_WATCHDOG_SUPPORT

#ifndef CONFIG_SPL_BUILD
# define CONFIG_TIMESTAMP
# define CONFIG_LZO
#endif

#define CONFIG_SYS_BOOTM_LEN		(16 << 20)

#define MACH_TYPE_TIAM335EVM		3589	/* Until the next sync */
#define CONFIG_MACH_TYPE		MACH_TYPE_TIAM335EVM
#define CONFIG_BOARD_LATE_INIT

/* Clock Defines */
#define V_OSCK				24000000  /* Clock output from T2 */
#define V_SCLK				(V_OSCK)

#include <config_distro_bootcmd.h>

#ifndef CONFIG_SPL_BUILD
#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_image=kernel.bin\0"	\
	"fdt_image=openwrt-nbhw16.dtb\0"	\
	"modeboot=sdboot\0" \
	"fdt_addr=0x82000000\0" \
	"kernel_addr=0x80000000\0" \
	"load_addr=0x83000000\0" \
	"root_part=1\0" /* Default root partition, overwritte in board/mv_ebu/a38x/nbhw14_env.c */ \
	"add_sd_bootargs=setenv bootargs $bootargs root=/dev/mmcblk0p$root_part rootfstype=ext4 console=ttyO0,115200 rootwait earlyprintk\0" \
	"add_version_bootargs=setenv bootargs $bootargs\0" \
	"fdt_skip_update=yes\0" \
    "ethprime=cpsw\0" \
	"sdbringup=echo Try bringup boot && ext4load mmc 1:$root_part $kernel_addr /boot/zImage && " \
			"ext4load mmc 1:$root_part $fdt_addr /boot/am335x-nbhw16.dtb && setenv bootargs $bootargs rw;\0" \
	"sdprod=ext4load mmc 1:$root_part $kernel_addr /boot/$kernel_image && " \
			"ext4load mmc 1:$root_part $fdt_addr /boot/$fdt_image && setenv bootargs $bootargs ro;\0" \
	"sdboot=if mmc dev 1; then " \
			"echo Copying Linux from SD to RAM... && " \
			"run sdprod || run sdbringup && " \
			"run add_sd_bootargs && run add_version_bootargs && bootz $kernel_addr - $fdt_addr; " \
		"fi\0" \
	"bootcmd=run sdboot\0" \
	"recovery=tftpboot $kernel_addr recovery-image; tftpboot $fdt_addr recovery-dtb; setenv bootargs rdinit=/etc/preinit console=ttyO0,115200 debug; bootz $kernel_addr - $fdt_addr\0"
#endif

/* NS16550 Configuration */
#define CONFIG_SYS_NS16550_COM1		0x44e09000	/* Base EVM has UART0 */
#define CONFIG_SYS_NS16550_COM2		0x48022000	/* UART1 */
#define CONFIG_SYS_NS16550_COM3		0x48024000	/* UART2 */
#define CONFIG_SYS_NS16550_COM4		0x481a6000	/* UART3 */
#define CONFIG_SYS_NS16550_COM5		0x481a8000	/* UART4 */
#define CONFIG_SYS_NS16550_COM6		0x481aa000	/* UART5 */
#define CONFIG_BAUDRATE			115200

#define CONFIG_CMD_EEPROM
#define CONFIG_SYS_I2C_EEPROM_ADDR              0x50	/* Main EEPROM */
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN          2
#define CONFIG_SYS_I2C_SPEED                    100000
#define CONFIG_SYS_EEPROM_PAGE_WRITE_BITS       4
#define CONFIG_SYS_EEPROM_PAGE_WRITE_DELAY_MS   50

#define CONFIG_ENV_IS_IN_EEPROM
#define CONFIG_ENV_OFFSET						0x1000  /* The Environment is located at 4k */
#define CONFIG_ENV_SIZE							0x800	/* The maximum size is 2k */
#undef  CONFIG_SPL_ENV_SUPPORT
#undef CONFIG_SPL_NAND_SUPPORT
#undef CONFIG_SPL_ONENAND_SUPPORT


/* We need to disable SPI to not confuse the eeprom env driver */
#undef CONFIG_SPI
#undef CONFIG_SPI_BOOT
#undef CONFIG_SPL_OS_BOOT

#define CONFIG_SPL_POWER_SUPPORT
#define CONFIG_SPL_YMODEM_SUPPORT

#define CONFIG_SPL_LDSCRIPT		"$(CPUDIR)/am33xx/u-boot-spl.lds"

#define CONFIG_SUPPORT_EMMC_BOOT

/*
 * USB configuration.  We enable MUSB support, both for host and for
 * gadget.  We set USB0 as peripheral and USB1 as host, based on the
 * board schematic and physical port wired to each.  Then for host we
 * add mass storage support and for gadget we add both RNDIS ethernet
 * and DFU.
 */
#define CONFIG_USB_MUSB_DSPS
#define CONFIG_ARCH_MISC_INIT
#define CONFIG_USB_MUSB_PIO_ONLY
#define CONFIG_USB_MUSB_DISABLE_BULK_COMBINE_SPLIT
#define CONFIG_AM335X_USB0
#define CONFIG_AM335X_USB0_MODE	MUSB_PERIPHERAL
#define CONFIG_AM335X_USB1
#define CONFIG_AM335X_USB1_MODE MUSB_HOST

/* Fastboot */
#define CONFIG_USB_FUNCTION_FASTBOOT
#define CONFIG_CMD_FASTBOOT
#define CONFIG_ANDROID_BOOT_IMAGE
#define CONFIG_FASTBOOT_BUF_ADDR	CONFIG_SYS_LOAD_ADDR
#define CONFIG_FASTBOOT_BUF_SIZE	0x07000000

/* To support eMMC booting */
#define CONFIG_STORAGE_EMMC
#define CONFIG_FASTBOOT_FLASH_MMC_DEV   1

#ifdef CONFIG_USB_MUSB_HOST
#define CONFIG_USB_STORAGE
#endif

#ifdef CONFIG_USB_MUSB_GADGET
/* Removing USB gadget and can be enabled adter adding support usb DM */
#ifndef CONFIG_DM_ETH
#define CONFIG_USB_ETHER
#define CONFIG_USB_ETH_RNDIS
#define CONFIG_USBNET_HOST_ADDR	"de:ad:be:af:00:00"
#endif /* CONFIG_DM_ETH */
#endif /* CONFIG_USB_MUSB_GADGET */

/*
 * Disable MMC DM for SPL build and can be re-enabled after adding
 * DM support in SPL
 */
#ifdef CONFIG_SPL_BUILD
#undef CONFIG_DM_MMC
#undef CONFIG_TIMER
#endif

#if defined(CONFIG_SPL_BUILD)
/* Remove other SPL modes. */
#undef CONFIG_SPL_NAND_SUPPORT
#define CONFIG_ENV_IS_NOWHERE
#undef CONFIG_PARTITION_UUIDS
#undef CONFIG_EFI_PARTITION
#endif

/* USB Device Firmware Update support */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_USB_FUNCTION_DFU
#define CONFIG_DFU_MMC
#define DFU_ALT_INFO_MMC \
	"dfu_alt_info_mmc=" \
	"boot part 0 1;" \
	"rootfs part 0 2;" \
	"MLO fat 0 1;" \
	"MLO.raw raw 0x100 0x100;" \
	"u-boot.img.raw raw 0x300 0x400;" \
	"spl-os-args.raw raw 0x80 0x80;" \
	"spl-os-image.raw raw 0x900 0x2000;" \
	"spl-os-args fat 0 1;" \
	"spl-os-image fat 0 1;" \
	"u-boot.img fat 0 1;" \
	"uEnv.txt fat 0 1\0"
#define DFU_ALT_INFO_NAND ""
#define CONFIG_DFU_RAM
#define DFU_ALT_INFO_RAM \
	"dfu_alt_info_ram=" \
	"kernel ram 0x80200000 0xD80000;" \
	"fdt ram 0x80F80000 0x80000;" \
	"ramdisk ram 0x81000000 0x4000000\0"
#define DFUARGS \
	"dfu_alt_info_emmc=rawemmc raw 0 3751936\0" \
	DFU_ALT_INFO_MMC \
	DFU_ALT_INFO_RAM \
	DFU_ALT_INFO_NAND
#endif

/* Network. */
#define CONFIG_PHY_GIGE
#define CONFIG_PHYLIB
#define CONFIG_PHY_SMSC

#ifdef CONFIG_DRIVER_TI_CPSW
#define CONFIG_CLOCK_SYNTHESIZER
#define CLK_SYNTHESIZER_I2C_ADDR 0x65
#endif

#define CONFIG_SYS_MEMTEST_START    0x80000000
#define CONFIG_SYS_MEMTEST_END      0x87900000

/* Enable support for TPS 65218 */
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_TPS65218
/* For compatibility reasons (BeagleBone) */
#define CONFIG_POWER_TPS65217
#define CONFIG_POWER_TPS62362

#endif	/* ! __CONFIG_AM335X_EVM_H */
