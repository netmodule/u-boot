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
#include "shield.h"
#include "shield_can.h"
#include "shield_comio.h"
#include "fileaccess.h"

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
#define NETBIRD_GPIO_PWR_GSM	GPIO_TO_PIN(1, 21)
#define NETBIRD_GPIO_SUPPLY_GSM	GPIO_TO_PIN(0, 31)
#define NETBIRD_GPIO_RST_GSM	GPIO_TO_PIN(1, 25)
#define NETBIRD_GPIO_WLAN_EN	GPIO_TO_PIN(3, 10)
#define NETBIRD_GPIO_BT_EN		GPIO_TO_PIN(3, 4)
#define NETBIRD_GPIO_EN_GPS_ANT	GPIO_TO_PIN(2, 24)
#define NETBIRD_GPIO_LED_A		GPIO_TO_PIN(1, 14)
#define NETBIRD_GPIO_LED_B		GPIO_TO_PIN(1, 15)
#define NETBIRD_GPIO_RESET_BUTTON	GPIO_TO_PIN(0, 2)
#define NETBIRD_GPIO_USB_PWR_EN		GPIO_TO_PIN(1, 27)
#define NETBIRD_GPIO_USB_PWR_EN_2	GPIO_TO_PIN(2, 4) // On new version this gpio is used

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
	.datardsratio0 = 0x39,
	.datawdsratio0 = 0x3f,
	.datafwsratio0 = 0x98,
	.datawrsratio0 = 0x7d,
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
		puts ("tps65218_reg_write failure (DCDC4 clear PFM Flag)\n");
	};

	/* Disable DCDC2 because it is not used and could make noise */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_ENABLE1, 0, 0x02)) {
		puts ("tps65218_reg_write failure (DCDC2 disable)\n");
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
	enable_uart1_pin_mux();
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

	/* Check if reset button is pressed for at least 2 seconds ≃ ~5s */
	do {
		if (gpio_get_value(NETBIRD_GPIO_RESET_BUTTON) != 0)  break;
		udelay(100000);  /* 100ms */
		counter++;

		if (counter==20) {/* Indicate factory reset threshold */
			gpio_set_value(NETBIRD_GPIO_LED_A, 0);
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
			udelay(400000);  /* 400ms */
			/* let LED blink up once */
			gpio_set_value(NETBIRD_GPIO_LED_A, 1);
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_A, 0);
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
		} else if (counter==120) { /* Indicate recovery boot threshold */
			/* let LED blink up twice */
			gpio_set_value(NETBIRD_GPIO_LED_A, 1);
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_A, 0);
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_A, 1);
			gpio_set_value(NETBIRD_GPIO_LED_B, 1);
			udelay(400000);  /* 400ms */
			gpio_set_value(NETBIRD_GPIO_LED_A, 0);
			gpio_set_value(NETBIRD_GPIO_LED_B, 0);
		}
	} while (counter<120);

	if (counter < 20) return 0; /* Don't do anything for duration < 2s */

	if (counter < 120) /* Do factory reset for duration between ~5s and ~15s */
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
		setenv("consoledev", "ttyS1");

		printf("Booting recovery image...\n");

		/* Set bootcmd to run recovery */
		setenv("bootcmd", "run recovery");

		return 0;
	}
	return 0;
}

static void enable_ext_usb(void)
{
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_USB_PWR_EN);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_USB_PWR_EN_2);
	/* Disable LS2 */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_ENABLE2, 0x00, 0x04)) {
		puts ("tps65218_reg_write failure (LS2 enable)\n");
	};

	/* Discharge LS2 to have proper 0V at the output */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_CONFIG3, 0x02, 0x02)) {
		puts ("tps65218_reg_write failure (LS2 discharge)\n");
	};

	mdelay(10);

	gpio_set_value(NETBIRD_GPIO_USB_PWR_EN, 1);
	gpio_set_value(NETBIRD_GPIO_USB_PWR_EN_2, 1);

	mdelay(50);

	/* Disable discharge LS2 */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_CONFIG3, 0x00, 0x02)) {
		puts ("tps65218_reg_write failure (LS2 discharge)\n");
	};

	/* Configure 500mA on LS2 */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_CONFIG2, 0x02, 0x03)) {
		puts ("tps65218_reg_write failure (LS2 enable)\n");
	};

	/* Enable LS2 */
	if (tps65218_reg_write(TPS65218_PROT_LEVEL_2, TPS65218_ENABLE2, 0x04, 0x04)) {
		puts ("tps65218_reg_write failure (LS2 enable)\n");
	};
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

	/* Remove power, and make sure reset is set once */
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_SUPPLY_GSM);
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_RST_GSM);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_PWR_GSM);
	mdelay(20);
	/* Enable gsm supply */
	gpio_set_value(NETBIRD_GPIO_SUPPLY_GSM, 1);
	mdelay(20);
	/* Take modem out of reset, we have to wait 300ms afterwards */
	gpio_set_value(NETBIRD_GPIO_RST_GSM, 0);
	mdelay(300);
	/* Do power up sequence, this modem has a special power up sequence
	 * where we have to pull PWR for > 1s but < 7s (see manual) */
	gpio_set_value(NETBIRD_GPIO_PWR_GSM, 1);
	mdelay(1200);
	gpio_set_value(NETBIRD_GPIO_PWR_GSM, 0);

	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_LED_A);
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_LED_B);
	REQUEST_AND_SET_GPIO(NETBIRD_GPIO_RST_PHY_N);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_WLAN_EN);
	REQUEST_AND_CLEAR_GPIO(NETBIRD_GPIO_BT_EN);

	/* There are two funcions on the same mux mode for MMC2_DAT7 we want
	 * to use RMII2_CRS_DV so we need to set SMA2 Register to 1
	 * See SPRS717J site 49 (10)*/
	#define SMA2_REGISTER (CTRL_BASE + 0x1320)
	writel(0x01, SMA2_REGISTER); /* Select RMII2_CRS_DV instead of MMC2_DAT7 */

	enable_ext_usb();

	printf("OSC:   %lu Hz\n", get_osclk());

	return 0;
}

/* Enable the ecap2 pwm see siemens/pxm2 */
static int enable_pwm(void)
{
#define PWM_TICKS	0xBEB
#define PWM_DUTY	0x5F5
#define AM33XX_ECAP2_BASE 0x48304100
#define PWMSS2_BASE 0x48304000
	struct pwmss_regs *pwmss = (struct pwmss_regs *)PWMSS2_BASE;
	struct pwmss_ecap_regs *ecap;
	int ticks = PWM_TICKS;
	int duty = PWM_DUTY;

	ecap = (struct pwmss_ecap_regs *)AM33XX_ECAP2_BASE;
	/* enable clock */
	setbits_le32(&pwmss->clkconfig, ECAP_CLK_EN);
	/* TimeStamp Counter register */
	writel(0x0, &ecap->ctrphs);

	setbits_le16(&ecap->ecctl2,
			 (ECTRL2_MDSL_ECAP | ECTRL2_SYNCOSEL_MASK));

	/* config period */
	writel(ticks - 1, &ecap->cap3);
	writel(ticks - 1, &ecap->cap1);
	/* config duty */
	writel(duty, &ecap->cap2);
	writel(duty, &ecap->cap4);
	/* start */
	setbits_le16(&ecap->ecctl2, ECTRL2_CTRSTP_FREERUN);
	return 0;
}

/* Enable the input clock for ecap2 and then enable the pwm */
static void enable_wlan_clock(void)
{
	struct cm_perpll *const cmper = (struct cm_perpll*)CM_PER;
	struct ctrl_dev *const cdev= (struct ctrl_dev*)CTRL_DEVICE_BASE;
	u32 *const clk_domains[] = { 0 };

	u32 *const clk_modules_nmspecific[] = {
		&cmper->epwmss2clkctrl,
		0
	};

	do_enable_clocks(clk_domains, clk_modules_nmspecific, 1);

	/* Enable timebase clock for pwmss2 */
	writel(0x04, &cdev->pwmssctrl);

	enable_pwm();
}

#if !defined(CONFIG_SPL_BUILD)

static void set_devicetree_name(void)
{
	char devicetreename[64];
	/* add hardware versions to environment */
	if (bd_get_devicetree(devicetreename, sizeof(devicetreename)) != 0) {
		printf("Devicetree name not found, use legacy name\n");
		strcpy(devicetreename, "am335x-nbhw16-prod2.dtb");
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

static void check_fct(void)
{
	/* If probe fails we are sure no eeprom is connected */
	if (i2c_probe(0x51) == 0) {
		printf("Entering fct mode\n");
		setenv ("bootcmd", "");
	}
}


static void set_fdtshieldcmd(const char *fdt_cmd)
{
	setenv("fdtshieldcmd", fdt_cmd);
}

struct shield_command {
	int shield_id;
	const char *name;
	const char *default_shieldcmd;
	const char *fdtshieldcmd;
};

#define SHIELD_COM_IO	0
#define SHIELD_DUALCAN	1

static struct shield_command known_shield_commands[] = {
	{
		SHIELD_COM_IO,
		"comio",
		"shield comio mode rs232",
		"fdt get value serial0 /aliases serial0;" \
		"fdt set $serial0 status okay"
	},
	{
		SHIELD_DUALCAN,
		"dualcan",
		"shield dualcan termination off off",
		"fdt get value can0 /aliases d_can0;" \
		"fdt get value can1 /aliases d_can1;" \
		"fdt set $can0 status okay;" \
		"fdt set $can1 status okay;" \
	},
};

static const struct shield_command* get_shield_command(int shield_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(known_shield_commands); i++) {
		if (known_shield_commands[i].shield_id == shield_id) {
			return &known_shield_commands[i];
		}
	}

	return NULL;
}

static void shield_config(void)
{
#define MAX_SHIELD_CMD_LEN 128
	char shieldcmd_linux[MAX_SHIELD_CMD_LEN];
	const char *shieldcmd;
	const struct shield_command *cmd;
	int len;

	int shield_id = bd_get_shield(0);
	if (shield_id < 0) {
		printf("No shield found in bd\n");
		return;
	}

	cmd = get_shield_command(shield_id);
	if (cmd == NULL) {
		printf ("Unknown shield id %d\n", shield_id);
		return;
	}

	printf("Shield found: %s\n", cmd->name);

	shieldcmd = cmd->default_shieldcmd;

	/* If a shield configuration set by linux take it without bd check, we asume that Linux knows
	 * what to do. */
	len = read_file("/root/boot/shieldcmd", shieldcmd_linux, MAX_SHIELD_CMD_LEN);
	if (len > 0) {
		debug("Shield command found in file, using it\n");
		shieldcmd = shieldcmd_linux;
	}

	printf("Shield command: %s\n", shieldcmd);

	setenv("shieldcmd", shieldcmd);

	set_fdtshieldcmd(cmd->fdtshieldcmd);
}

static void shield_init(void)
{
	can_shield_init();
	comio_shield_init();

	shield_config();
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
	fs_set_console();

	check_reset_button();

	get_hw_version();

	set_devicetree_name();
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	int rc;
	char *name = NULL;

	set_board_info_env(name);
#endif

	enable_wlan_clock();

#if !defined(CONFIG_SPL_BUILD)
	shield_init();

	check_fct();

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
	__maybe_unused struct ti_am_eeprom *header;

#if !defined(CONFIG_SPL_BUILD)
#ifdef CONFIG_DRIVER_TI_CPSW

	cpsw_data.mdio_div = 0x3E;

	bd_get_mac(0, mac_addr0, sizeof(mac_addr0));
	set_mac_address(0, mac_addr0);

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
