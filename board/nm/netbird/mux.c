/*
 * mux.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mux.h>
#include <asm/io.h>
#include <i2c.h>
#include "board.h"

static struct module_pin_mux uart2_pin_mux[] = {
	{OFFSET(spi0_sclk), (MODE(1) | PULLUP_EN | RXACTIVE)},	/* UART2_RXD */
	{OFFSET(spi0_d0), (MODE(1) | PULLUDEN)},		/* UART2_TXD */
	{-1},
};

static struct module_pin_mux uart3_pin_mux[] = {
	{OFFSET(spi0_cs1), (MODE(1) | PULLUP_EN | RXACTIVE)},	/* UART3_RXD */
	{OFFSET(ecap0_in_pwm0_out), (MODE(1) | PULLUDEN)},	/* UART3_TXD */
	{-1},
};

static struct module_pin_mux uart4_pin_mux[] = {
	{OFFSET(gpmc_wait0), (MODE(6) | PULLUP_EN | RXACTIVE)},	/* UART4_RXD */
	{OFFSET(gpmc_wpn), (MODE(6) | PULLUDEN)},		/* UART4_TXD */
	{-1},
};

static struct module_pin_mux uart5_pin_mux[] = {
	{OFFSET(lcd_data9), (MODE(4) | PULLUP_EN | RXACTIVE)},	/* UART5_RXD */
	{OFFSET(lcd_data8), (MODE(4) | PULLUDEN)},		/* UART5_TXD */
	{-1},
};

static struct module_pin_mux i2c0_pin_mux[] = {
	{OFFSET(i2c0_sda), (MODE(0) | RXACTIVE |
			PULLUDEN | PULLUP_EN | SLEWCTRL)}, /* I2C_DATA */
	{OFFSET(i2c0_scl), (MODE(0) | RXACTIVE |
			PULLUDEN | PULLUP_EN | SLEWCTRL)}, /* I2C_SCLK */
	{-1},
};



static struct module_pin_mux uart0_netbird_pin_mux[] = {
	{OFFSET(uart0_rxd), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* UART0_RXD */
	{OFFSET(uart0_txd), (MODE(0) | PULLUDEN | PULLUP_EN)},		/* UART0_TXD */
	{-1},
};

static struct module_pin_mux uart1_netbird_pin_mux[] = {
	{OFFSET(uart1_rxd), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* (D16) uart1_rxd.uart1_rxd */
	{OFFSET(uart1_txd), (MODE(0) | PULLUDEN | PULLUP_EN)},				/* (D15) uart1_txd.uart1_txd */
	{OFFSET(uart1_ctsn), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* (D18) uart1_ctsn.uart1_ctsn */
	{OFFSET(uart1_rtsn), (MODE(0) | PULLUDEN | PULLUP_EN)},				/* (D17) uart1_rtsn.uart1_rtsn */
	{-1},
};

static struct module_pin_mux rmii0_netbird_pin_mux[] = {
	{OFFSET(mii1_crs), MODE(1) | PULLUDDIS | RXACTIVE},		/* MII1_CRS */
	{OFFSET(mii1_rxerr), MODE(1) | PULLUDDIS | RXACTIVE},	/* MII1_RXERR */
	{OFFSET(mii1_txen), MODE(1) | PULLUDDIS },			/* MII1_TXEN */
	{OFFSET(mii1_txd0), MODE(1) | PULLUDDIS },			/* MII1_TXD0 */
	{OFFSET(mii1_txd1), MODE(1) | PULLUDDIS },			/* MII1_TXD1 */
	{OFFSET(mii1_rxd0), MODE(1) | PULLUDDIS | RXACTIVE },	/* MII1_RXD0 */
	{OFFSET(mii1_rxd1), MODE(1) | PULLUDDIS | RXACTIVE },	/* MII1_RXD1 */
	{OFFSET(rmii1_refclk), MODE(0) | PULLUDDIS | RXACTIVE},	/* RMII1_REFCLK */
	{OFFSET(mdio_clk), MODE(0) | PULLUDDIS },	/* MDIO_CLK */
	{OFFSET(mdio_data), MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE }, /* MDIO_DATA */
	{OFFSET(xdma_event_intr0), MODE(3) }, /* CLK_OUT1 for MDIO (design option) */
	{-1},
};

static struct module_pin_mux rmii1_netbird_pin_mux[] = {
	{OFFSET(gpmc_a9), MODE(3) | PULLUDDIS | RXACTIVE},		/* MII2_CRS */
	{OFFSET(gpmc_wpn), MODE(3) | PULLUDDIS | RXACTIVE},	/* MII2_RXERR */
	{OFFSET(gpmc_a0), MODE(3) | PULLUDDIS},			/* MII2_TXEN */
	{OFFSET(gpmc_a5), MODE(3) | PULLUDDIS},			/* MII2_TXD0 */
	{OFFSET(gpmc_a4), MODE(3) | PULLUDDIS},			/* MII2_TXD1 */
	{OFFSET(gpmc_a11), MODE(3) | PULLUDDIS | RXACTIVE},	/* MII1_RXD0 */
	{OFFSET(gpmc_a10), MODE(3) | PULLUDDIS | RXACTIVE},	/* MII1_RXD1 */
	{OFFSET(mii1_col), MODE(1) | PULLUDDIS | RXACTIVE},	/* RMII1_REFCLK */
	{-1},
};

static struct module_pin_mux mmc0_sdio_netbird_pin_mux[] = {
	{OFFSET(mmc0_clk), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* MMC0_CLK */
	{OFFSET(mmc0_cmd), (MODE(0) | PULLUDEN | PULLUP_EN)},	/* MMC0_CMD */
	{OFFSET(mmc0_dat0), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC0_DAT0 */
	{OFFSET(mmc0_dat1), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC0_DAT1 */
	{OFFSET(mmc0_dat2), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC0_DAT2 */
	{OFFSET(mmc0_dat3), (MODE(0) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC0_DAT3 */
	{-1},
};

static struct module_pin_mux mmc1_emmc_netbird_pin_mux[] = {
	{OFFSET(gpmc_csn1), (MODE(2) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* MMC1_CLK */
	{OFFSET(gpmc_csn2), (MODE(2) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* MMC1_CMD */
	{OFFSET(gpmc_ad0), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT0 */
	{OFFSET(gpmc_ad1), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT1 */
	{OFFSET(gpmc_ad2), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT2 */
	{OFFSET(gpmc_ad3), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT3 */
	{OFFSET(gpmc_ad4), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT3 */
	{OFFSET(gpmc_ad5), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT3 */
	{OFFSET(gpmc_ad6), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT3 */
	{OFFSET(gpmc_ad7), (MODE(1) | PULLUDEN | PULLUP_EN | RXACTIVE )},	/* MMC1_DAT3 */
	{-1},
};

static struct module_pin_mux gpio_netbird_pin_mux[] = {
	/* Bank 0 */
	{OFFSET(ecap0_in_pwm0_out), (MODE(7) | PULLUDDIS | RXACTIVE)},	/* (C18) eCAP0_in_PWM0_out.gpio0[7] */  /* PWM */
	{OFFSET(mii1_txd3), (MODE(7) | PULLUDDIS)},	/* (J18) gmii1_txd3.gpio0[16] */  /* RST_PHY~ */
	{OFFSET(gpmc_ad11), (MODE(7) | PULLUDDIS)},	/* (U12) gpmc_ad11.gpio0[27] */  /* RST_EXT~ */
	/* Bank 1 */
	{OFFSET(gpmc_ad13), (MODE(7) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* (R12) gpmc_ad13.gpio1[13] */  /* BUTTON */
	{OFFSET(gpmc_ad14), (MODE(7) | PULLUDDIS)},	/* (V13) gpmc_ad14.gpio1[14] */  /* LED_A */
	{OFFSET(gpmc_ad15), (MODE(7) | PULLUDDIS)},	/* (U13) gpmc_ad15.gpio1[15] */  /* LED_B */
	{OFFSET(gpmc_a6), (MODE(7) | PULLUDDIS)},	/* (U15) gpmc_a6.gpio1[22] */  /* GSM_PWR_EN */
	{OFFSET(gpmc_a8), (MODE(7) | PULLUDDIS)},	/* (V16) gpmc_a8.gpio1[24] */  /* RST_GSM~ */
	/* Bank 2 */
	{OFFSET(lcd_pclk), (MODE(7) | PULLUDDIS)},	/* (V5) lcd_pclk.gpio2[24] */  /* EN_GPS_ANT */
	{OFFSET(lcd_data3), (MODE(7) | PULLUDEN| PULLUP_EN)},	/* (V5) lcd_pclk.gpio2[9] */  /* SYSBOOT */
	{OFFSET(lcd_data4), (MODE(7) | PULLUDEN| PULLUP_EN)},	/* (V5) lcd_pclk.gpio2[10] */  /* SYSBOOT */
	/* Bank 3 */
	{OFFSET(mii1_rxdv), (MODE(7) | PULLUDDIS)},	/* (J17) gmii1_rxdv.gpio3[4] */  /* BT_EN */
	{OFFSET(mii1_rxdv), (MODE(7) | PULLUDEN | PULLUP_EN | RXACTIVE)},	/* (K18) gmii1_txclk.gpio3[9] */  /* WLAN_IRQ */
	{OFFSET(mii1_rxdv), (MODE(7) | PULLUDDIS)},	/* (L18) gmii1_rxclk.gpio3[10] */  /* WLAN_EN */
	{-1},
};

static struct module_pin_mux usb_netbird_pin_mux[] = {
	{OFFSET(usb0_drvvbus), (MODE(0) | PULLUDEN | PULLDOWN_EN)},	/* (F16) USB0_DRVVBUS.USB0_DRVVBUS */  /* PWM */
	{OFFSET(usb1_drvvbus), (MODE(0) | PULLUDDIS | PULLDOWN_EN)},	/* (F15) USB1_DRVVBUS.USB1_DRVVBUS */  /* RST_PHY~ */
	{-1},
};

static struct module_pin_mux unused_netbird_pin_mux[] = {
	{OFFSET(lcd_data6), (MODE(7) | PULLUDEN | PULLDOWN_EN)},	/* SYSBOOT6 is not used bulldown active, receiver disabled */
	{OFFSET(lcd_data7), (MODE(7) | PULLUDEN | PULLDOWN_EN)},	/* SYSBOOT7 is not used bulldown active, receiver disabled */
	{OFFSET(lcd_data10), (MODE(7) | PULLUDEN | PULLDOWN_EN)},	/* SYSBOOT10 is not used bulldown active, receiver disabled */
	{OFFSET(lcd_data11), (MODE(7) | PULLUDEN | PULLDOWN_EN)},	/* SYSBOOT11 is not used bulldown active, receiver disabled */
	{-1},
};

void enable_uart0_pin_mux(void)
{
	configure_module_pin_mux(uart0_netbird_pin_mux);
}

void enable_uart1_pin_mux(void)
{
	configure_module_pin_mux(uart1_netbird_pin_mux);
}

void enable_uart2_pin_mux(void)
{
	configure_module_pin_mux(uart2_pin_mux);
}

void enable_uart3_pin_mux(void)
{
	configure_module_pin_mux(uart3_pin_mux);
}

void enable_uart4_pin_mux(void)
{
	configure_module_pin_mux(uart4_pin_mux);
}

void enable_uart5_pin_mux(void)
{
	configure_module_pin_mux(uart5_pin_mux);
}

void enable_i2c0_pin_mux(void)
{
	configure_module_pin_mux(i2c0_pin_mux);
}

/*
 * The AM335x GP EVM, if daughter card(s) are connected, can have 8
 * different profiles.  These profiles determine what peripherals are
 * valid and need pinmux to be configured.
 */
#define PROFILE_NONE	0x0
#define PROFILE_0	(1 << 0)
#define PROFILE_1	(1 << 1)
#define PROFILE_2	(1 << 2)
#define PROFILE_3	(1 << 3)
#define PROFILE_4	(1 << 4)
#define PROFILE_5	(1 << 5)
#define PROFILE_6	(1 << 6)
#define PROFILE_7	(1 << 7)
#define PROFILE_MASK	0x7
#define PROFILE_ALL	0xFF

/* CPLD registers */
#define I2C_CPLD_ADDR	0x35
#define CFG_REG		0x10

void enable_board_pin_mux(void)
{
	/* Netbird board */
	configure_module_pin_mux(gpio_netbird_pin_mux);
	configure_module_pin_mux(rmii0_netbird_pin_mux);
	configure_module_pin_mux(rmii1_netbird_pin_mux);
	configure_module_pin_mux(mmc0_sdio_netbird_pin_mux);
	configure_module_pin_mux(mmc1_emmc_netbird_pin_mux);
	configure_module_pin_mux(usb_netbird_pin_mux);
	configure_module_pin_mux(usb_netbird_pin_mux);
	configure_module_pin_mux(i2c0_pin_mux);
	configure_module_pin_mux(unused_netbird_pin_mux);
}