/*
 * board.c
 *
 * Board functions for TI AM335X based boards
 *
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <spl.h>
#include <serial.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/omap.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/clock.h>
#include <asm/arch/clk_synthesizer.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mem.h>
#include <asm/io.h>
#include <asm/emif.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <miiphy.h>
#include <cpsw.h>
#include <power/tps65217.h>
#include <power/tps65218.h>
#include <power/tps65910.h>
#include <environment.h>
#include <watchdog.h>
#include <environment.h>
#include "../common/bdparser.h"
#include "../common/board_descriptor.h"
#include "board.h"

DECLARE_GLOBAL_DATA_PTR;

/* GPIO that controls power to DDR on EVM-SK */
#define GPIO_TO_PIN(bank, gpio)		(32 * (bank) + (gpio))
#define GPIO_DDR_VTT_EN		GPIO_TO_PIN(0, 7)
#define ICE_GPIO_DDR_VTT_EN	GPIO_TO_PIN(0, 18)
#define GPIO_PR1_MII_CTRL	GPIO_TO_PIN(3, 4)
#define GPIO_MUX_MII_CTRL	GPIO_TO_PIN(3, 10)
#define GPIO_FET_SWITCH_CTRL	GPIO_TO_PIN(0, 7)
#define GPIO_PHY_RESET		GPIO_TO_PIN(2, 5)

#define NETBIRD_GPIO_RST_PHY_N	GPIO_TO_PIN(0, 16)
#define NETBIRD_GPIO_PWR_GSM	GPIO_TO_PIN(1, 22)
#define NETBIRD_GPIO_RST_GSM	GPIO_TO_PIN(1, 24)
#define NETBIRD_GPIO_WLAN_EN	GPIO_TO_PIN(3, 10)
#define NETBIRD_GPIO_BT_EN		GPIO_TO_PIN(3, 4)
#define NETBIRD_GPIO_EN_GPS_ANT	GPIO_TO_PIN(2, 24)
#define NETBIRD_GPIO_LED_A		GPIO_TO_PIN(1, 14)
#define NETBIRD_GPIO_LED_B		GPIO_TO_PIN(1, 15)
#define NETBIRD_GPIO_RESET_BUTTON	GPIO_TO_PIN(1, 13)

#define DDR3_CLOCK_FREQUENCY (400)

#if defined(CONFIG_SPL_BUILD) || \
	(defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_DM_ETH))
static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;
#endif

#define BD_EEPROM_ADDR			(0x50)	/* CPU BD EEPROM (8kByte) is at 50 (A0) */
#define BD_ADDRESS				(0x0000)  /* Board descriptor at beginning of EEPROM */
#define PD_ADDRESS				(0x0200)  /* Product descriptor */
#define PARTITION_ADDRESS		(0x0600)  /* Partition Table */

static BD_Context   bdctx[3];		/* The descriptor context */

static int _bd_init(void)
{
	if (bd_get_context(&bdctx[0], BD_EEPROM_ADDR, BD_ADDRESS) != 0) {
		printf("%s() no valid bd found\n", __func__);
		return -1;
	}

	if (bd_get_context(&bdctx[1], BD_EEPROM_ADDR, PD_ADDRESS) != 0) {
		printf("%s() no valid pd found (legacy support)\n", __func__);
	}

	if (bd_get_context(&bdctx[2], BD_EEPROM_ADDR, PARTITION_ADDRESS) != 0) {
		printf("%s() no valid partition table found\n", __func__);
	}

	bd_register_context_list(bdctx, ARRAY_SIZE(bdctx));
    return 0;
}

/*
 * Read header information from EEPROM into global structure.
 */
static inline int __maybe_unused read_eeprom(void)
{
    return _bd_init();
}

struct serial_device *default_serial_console(void)
{
	return &eserial1_device;
}

#ifndef CONFIG_SKIP_LOWLEVEL_INIT

static const struct ddr_data ddr3_netbird_data = {
	/* Ratios were optimized by DDR3 training software from TI */
	.datardsratio0 = 0x37,
	.datawdsratio0 = 0x42,
	.datafwsratio0 = 0x98,
	.datawrsratio0 = 0x7a,
};

static const struct cmd_control ddr3_netbird_cmd_ctrl_data = {
	.cmd0csratio = MT41K256M16HA125E_RATIO,
	.cmd0iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd1csratio = MT41K256M16HA125E_RATIO,
	.cmd1iclkout = MT41K256M16HA125E_INVERT_CLKOUT,

	.cmd2csratio = MT41K256M16HA125E_RATIO,
	.cmd2iclkout = MT41K256M16HA125E_INVERT_CLKOUT,
};

static struct emif_regs ddr3_netbird_emif_reg_data = {
	.sdram_config = MT41K256M16HA125E_EMIF_SDCFG,
	.ref_ctrl = 0x61A,	/* 32ms > 85°C */
	.sdram_tim1 = 0x0AAAE51B,
	.sdram_tim2 = 0x246B7FDA,
	.sdram_tim3 = 0x50FFE67F,
	.zq_config = MT41K256M16HA125E_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41K256M16HA125E_EMIF_READ_LATENCY,
};


#ifdef CONFIG_SPL_OS_BOOT
int spl_start_uboot(void)
{
	/* break into full u-boot on 'c' */
	if (serial_tstc() && serial_getc() == 'c')
		return 1;

#ifdef CONFIG_SPL_ENV_SUPPORT
	env_init();
	env_relocate_spec();
	if (getenv_yesno("boot_os") != 1)
		return 1;
#endif

	return 0;
}
#endif

#define OSC	(V_OSCK/1000000)
struct dpll_params dpll_ddr_nbhw16= {
		DDR3_CLOCK_FREQUENCY, OSC-1, 1, -1, -1, -1, -1};

void am33xx_spl_board_init(void)
{
	/* Get the frequency */
	dpll_mpu_opp100.m = am335x_get_efuse_mpu_max_freq(cdev);

	/* Set CPU speed to 600 MHZ */
	dpll_mpu_opp100.m = MPUPLL_M_600;

	/* Set CORE Frequencies to OPP100 */
	do_setup_dpll(&dpll_core_regs, &dpll_core_opp100);

	/* Clear th PFM Flag on DCDC4 */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_DCDC4, 0x00, 0x80)) {
		puts ("tps65218_reg_write failure\n");
	};

	/* Set MPU Frequency to what we detected now that voltages are set */
	do_setup_dpll(&dpll_mpu_regs, &dpll_mpu_opp100);

	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");
}

const struct dpll_params *get_dpll_ddr_params(void)
{
	dpll_ddr_nbhw16.n = (get_osclk() / 1000000) - 1;
	return &dpll_ddr_nbhw16;
}

void set_uart_mux_conf(void)
{
	enable_uart0_pin_mux();
}

void set_mux_conf_regs(void)
{
	enable_board_pin_mux();
}


const struct ctrl_ioregs ioregs_netbird = {
	.cm0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.cm2ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt0ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
	.dt1ioctl		= MT41K256M16HA125E_IOCTRL_VALUE,
};


void sdram_init(void)
{
	config_ddr(DDR3_CLOCK_FREQUENCY, &ioregs_netbird,
		   &ddr3_netbird_data,
		   &ddr3_netbird_cmd_ctrl_data,
		   &ddr3_netbird_emif_reg_data, 0);
}

#endif /* CONFIG_SKIP_LOWLEVEL_INIT */

static void request_and_set_gpio(int gpio, char *name, int value)
{
	int ret;

	ret = gpio_request(gpio, name);
	if (ret < 0) {
		printf("%s: Unable to request %s\n", __func__, name);
		return;
	}

	ret = gpio_direction_output(gpio, 0);
	if (ret < 0) {
		printf("%s: Unable to set %s  as output\n", __func__, name);
		goto err_free_gpio;
	}

	gpio_set_value(gpio, value);

	return;

err_free_gpio:
	gpio_free(gpio);
}

#define REQUEST_AND_SET_GPIO(N)	request_and_set_gpio(N, #N, 1);
#define REQUEST_AND_CLEAR_GPIO(N)	request_and_set_gpio(N, #N, 0);


int check_reset_button(void)
{
	int counter = 0;
	int ret;

	ret = gpio_request(NETBIRD_GPIO_RESET_BUTTON, "reset button");
	if (ret < 0) {
		printf("Unable to request reset button gpio\n");
		return -1;
	}

	ret = gpio_direction_input(NETBIRD_GPIO_RESET_BUTTON);
	if (ret < 0) {
		printf("Unable to set reset button as input\n");
		return -1;
	}

	/* Check if reset button is pressed for at least 3 seconds */
	do {
		if (gpio_get_value(NETBIRD_GPIO_RESET_BUTTON) != 0)  break;
		udelay(100000);  /* 100ms */
		counter++;

		if (counter==30) {/* Indicate factory reset threshold */
			/* let LED blink up once */
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
		} else if (counter==150) { /* Indicate recovery boot threshold */
			/* let LED blink up twice */
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
		}
	} while (counter<150);

	if (counter < 30) return 0; /* Don't do anything for duration < 3s */

	if (counter < 150) /* Do factory reset for duration between 3s and 15s */
	{
		char new_bootargs[512];
		char *bootargs = getenv("bootargs");

		if (bootargs==0) bootargs="";

		printf("Do factory reset during boot...\n");

		strncpy(new_bootargs, bootargs, sizeof(new_bootargs));
		strncat(new_bootargs, " factory-reset", sizeof(new_bootargs));

		setenv("bootargs", new_bootargs);

		printf("bootargs = %s\n", new_bootargs);

		return 1;
	} else {	/* Boot into recovery for duration > 15s */

		/* set consoledev to external port */
		setenv("consoledev", "ttyO0");

		printf("Booting recovery image...\n");

		/* Set bootcmd to run recovery */
		setenv("bootcmd", "run recovery");

		return 0;
	}
	return 0;
}

/*
 * Basic board specific setup.  Pinmux has been handled already.
 */
int board_init(void)
{
#if defined(CONFIG_HW_WATCHDOG)
	hw_watchdog_init();
#endif

	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
#if defined(CONFIG_NOR) || defined(CONFIG_NAND)
	gpmc_init();
#endif

	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_RST_GSM);
	udelay(10000);
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_PWR_GSM);
	mdelay(1200);
	gpio_set_value(NETBIRD_GPIO_PWR_GSM, 0);
	/* Enable debug LED to troubleshoot hw problems */
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_LED_A);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_LED_B);
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_RST_PHY_N);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_WLAN_EN);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_BT_EN);
	/* There are two funcions on the same mux mode for MMC2_DAT7 we want
	 * to use RMII2_CRS_DV so we need to set SMA2 Register to 1
	 * See SPRS717J site 49 (10)*/
	#define SMA2_REGISTER (CTRL_BASE + 0x1320)
	writel(0x01, SMA2_REGISTER); /* Select RMII2_CRS_DV instead of MMC2_DAT7 */

	printf("OSC: %lu Hz\n", get_osclk());

	return 0;
}
#if !defined(CONFIG_SPL_BUILD)

static void set_devicetree_name(void)
{
	char devicetreename[64];
	/* add hardware versions to environment */
	if (bd_get_devicetree(devicetreename, sizeof(devicetreename)) != 0) {
		printf("Devicetree name not found, use legacy name\n");
		strcpy(devicetreename, "am335x-nbhw16.dtb");
	}

	setenv("fdt_image", devicetreename);
}

static void get_hw_version(void)
{
	int hw_ver, hw_rev;
	char hw_versions[16];
	char new_env[256];

	/* add hardware versions to environment */
	bd_get_hw_version(&hw_ver, &hw_rev);
	printf("HW16:  V%d.%d\n", hw_ver, hw_rev);
	snprintf(hw_versions, sizeof(hw_versions), "CP=%d.%d", hw_ver, hw_rev);
	snprintf(new_env, sizeof(new_env), "setenv bootargs $bootargs %s", hw_versions);
	setenv("add_version_bootargs", new_env);
}

#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#if !defined(CONFIG_SPL_BUILD)
	int boot_partition;

	if (read_eeprom() < 0)
		puts("Could not get board ID.\n");

	/* add active root partition to environment */
	boot_partition = bd_get_boot_partition();
	if (boot_partition > 1) {
		boot_partition = 0;
	}

	/* mmcblk0p1 => root0, mmcblk0p2 => root1 so +1 */
	setenv_ulong("root_part", boot_partition + 1);

	check_reset_button();

	get_hw_version();

	set_devicetree_name();
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	int rc;
	char *name = NULL;

	set_board_info_env(name);
#endif

	return 0;
}
#endif

#ifndef CONFIG_DM_ETH

#if (defined(CONFIG_DRIVER_TI_CPSW) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_ETH_SUPPORT) && defined(CONFIG_SPL_BUILD))
static void cpsw_control(int enabled)
{
	/* VTP can be added here */

	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr	= 0,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_addr	= 1,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};
#endif

#if ((defined(CONFIG_SPL_ETH_SUPPORT) || defined(CONFIG_SPL_USBETH_SUPPORT)) &&\
	defined(CONFIG_SPL_BUILD)) || \
	((defined(CONFIG_DRIVER_TI_CPSW) || \
	  defined(CONFIG_USB_ETHER) && defined(CONFIG_MUSB_GADGET)) && \
	 !defined(CONFIG_SPL_BUILD))

static void set_mac_address(int index, uchar mac[6])
{
	/* Then take mac from bd */
	if (is_valid_ethaddr(mac)) {
		eth_setenv_enetaddr_by_index("eth", index, mac);
	}
	else {
		printf("Trying to set invalid MAC address");
	}
}

/*
 * This function will:
 * Read the eFuse for MAC addresses, and set ethaddr/eth1addr/usbnet_devaddr
 * in the environment
 * Perform fixups to the PHY present on certain boards.  We only need this
 * function in:
 * - SPL with either CPSW or USB ethernet support
 * - Full U-Boot, with either CPSW or USB ethernet
 * Build in only these cases to avoid warnings about unused variables
 * when we build an SPL that has neither option but full U-Boot will.
 */
int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
	uint8_t mac_addr0[6] = {02,00,00,00,00,01};
	uint8_t mac_addr1[6] = {02,00,00,00,00,02};
	__maybe_unused struct ti_am_eeprom *header;

#if !defined(CONFIG_SPL_BUILD)
#ifdef CONFIG_DRIVER_TI_CPSW

	cpsw_data.mdio_div = 0x3E;

	bd_get_mac(0, mac_addr0, sizeof(mac_addr0));
	set_mac_address(0, mac_addr0);

	bd_get_mac(1, mac_addr1, sizeof(mac_addr1));
	set_mac_address(1, mac_addr1);

	writel(RMII_MODE_ENABLE | RMII_CHIPCKL_ENABLE, &cdev->miisel);
	cpsw_slaves[0].phy_if = PHY_INTERFACE_MODE_RMII;
	cpsw_slaves[1].phy_if = PHY_INTERFACE_MODE_RMII;
	cpsw_slaves[0].phy_addr = 0;
	cpsw_slaves[1].phy_addr = 1;

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;
#endif

#endif
#if defined(CONFIG_USB_ETHER) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USBETH_SUPPORT))
	if (is_valid_ethaddr(mac_addr0))
		eth_setenv_enetaddr("usbnet_devaddr", mac_addr0);

	rv = usb_eth_initialize(bis);
	if (rv < 0)
		printf("Error %d registering USB_ETHER\n", rv);
	else
		n += rv;
#endif
	return n;
}
#endif

#endif /* CONFIG_DM_ETH */

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	return 0;
}
#endif
