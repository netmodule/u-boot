/*
 * Library to support early TI EVM EEPROM handling
 *
 * Copyright (C) 2015-2016 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla
 *	Steve Kipisz
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <asm/omap_common.h>
#include <i2c.h>

#include "board_detect.h"

int __maybe_unused ti_i2c_eeprom_am_get(int bus_addr, int dev_addr)
{
	struct ti_common_eeprom *ep;

	ep = TI_EEPROM_DATA;
	if (ep->header == TI_EEPROM_HEADER_MAGIC)
		goto already_read;

	/* Initialize with a known bad marker for i2c fails.. */
	ep->header = TI_EEPROM_HEADER_MAGIC;
	strlcpy(ep->name, "NBHW16", TI_EEPROM_HDR_NAME_LEN + 1);
	strlcpy(ep->version, "0.0", TI_EEPROM_HDR_REV_LEN + 1);
	strlcpy(ep->serial, "1234", TI_EEPROM_HDR_SERIAL_LEN + 1);
	strlcpy(ep->config, "", TI_EEPROM_HDR_CONFIG_LEN + 1);

	memset(ep->mac_addr, 0x00, TI_EEPROM_HDR_NO_OF_MAC_ADDR * TI_EEPROM_HDR_ETH_ALEN);

already_read:
	return 0;
}

bool __maybe_unused board_ti_is(char *name_tag)
{
	struct ti_common_eeprom *ep = TI_EEPROM_DATA;

	if (ep->header == TI_DEAD_EEPROM_MAGIC)
		return false;
	return !strncmp(ep->name, name_tag, TI_EEPROM_HDR_NAME_LEN);
}

char * __maybe_unused board_ti_get_rev(void)
{
	struct ti_common_eeprom *ep = TI_EEPROM_DATA;

	if (ep->header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return ep->version;
}

char * __maybe_unused board_ti_get_config(void)
{
	struct ti_common_eeprom *ep = TI_EEPROM_DATA;

	if (ep->header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return ep->config;
}

char * __maybe_unused board_ti_get_name(void)
{
	struct ti_common_eeprom *ep = TI_EEPROM_DATA;

	if (ep->header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return ep->name;
}

void __maybe_unused set_board_info_env(char *name)
{
	return;
}
