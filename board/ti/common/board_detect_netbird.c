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
#include "bdparser.h"

#define BD_ADDRESS                0x0000  /* Board descriptor at beginning of EEPROM */

/**
 * ti_i2c_eeprom_init - Initialize an i2c bus and probe for a device
 * @i2c_bus: i2c bus number to initialize
 * @dev_addr: Device address to probe for
 *
 * Return: 0 on success or corresponding error on failure.
 */
static int __maybe_unused ti_i2c_eeprom_init(int i2c_bus, int dev_addr)
{
	int rc;

	if (i2c_bus >= 0) {
		rc = i2c_set_bus_num(i2c_bus);
		if (rc)
			return rc;
	}

	return i2c_probe(dev_addr);
}

/**
 * ti_i2c_eeprom_read - Read data from an EEPROM
 * @dev_addr: The device address of the EEPROM
 * @offset: Offset to start reading in the EEPROM
 * @ep: Pointer to a buffer to read into
 * @epsize: Size of buffer
 *
 * Return: 0 on success or corresponding result of i2c_read
 */
static int __maybe_unused ti_i2c_eeprom_read(int dev_addr, int offset,
					     uchar *ep, int epsize)
{
	return i2c_read(dev_addr, offset, 2, ep, epsize);
}

int __maybe_unused ti_i2c_eeprom_am_get(int bus_addr, int dev_addr)
{
	struct ti_common_eeprom *ep;
	BD_Context  bdCtx;        /* The board descriptor context */
	u8          bdHeader[8];
	void*       pBdData = NULL;
	u8          bdHwVer     = 0;
	u8          bdHwRev     = 0;
	char        bdProdName[32];
	bd_bool_t   rc;

	ep = TI_EEPROM_DATA;
	if (ep->header == TI_EEPROM_HEADER_MAGIC)
		goto already_read;

	/* Mark eeprom valid. */
	ep->header = TI_EEPROM_HEADER_MAGIC;
	strlcpy(ep->name, "NBHW16", TI_EEPROM_HDR_NAME_LEN + 1); /* Do not take from BD to allow use of u-boot without BD. */
	strlcpy(ep->version, "0.0", TI_EEPROM_HDR_REV_LEN + 1);
	strlcpy(ep->serial, "", TI_EEPROM_HDR_SERIAL_LEN + 1);
	strlcpy(ep->config, "", TI_EEPROM_HDR_CONFIG_LEN + 1);

	gpi2c_init();
	rc = ti_i2c_eeprom_init(bus_addr, dev_addr);
	if (rc)
		goto do_fake_bd;

	/* Read header bytes from beginning of EEPROM */
	if (i2c_read( dev_addr, BD_ADDRESS, 2, bdHeader, BD_HEADER_LENGTH )) {
		printf("%s() Can't read BD header from EEPROM\n", __FUNCTION__);
		goto do_fake_bd;
	}

	/* Check whether this is a valid board descriptor (or empty EEPROM) */
	rc = BD_CheckHeader( &bdCtx, bdHeader );
	if (!rc) {
		printf("%s() No valid board descriptor found\n", __FUNCTION__);
		goto do_fake_bd;
	}

	/* Allocate memory for descriptor data and .. */
	pBdData = malloc( bdCtx.size );
	if ( pBdData == NULL ) {
		printf("%s() Can't allocate %d bytes\n", __FUNCTION__, bdCtx.size);
		goto do_fake_bd;
	}

	/* .. read data from EEPROM */
	if (i2c_read(dev_addr, BD_ADDRESS+BD_HEADER_LENGTH, 2, pBdData, bdCtx.size)) {
		printf("%s() Can't read data from EEPROM\n", __FUNCTION__);
		goto do_fake_bd;
	}

	/*
	 * Import data into board descriptor context
	 */
	rc = BD_ImportData( &bdCtx, pBdData );
	if (!rc) {
		printf("%s() Invalid board descriptor data\n", __FUNCTION__);
		goto do_fake_bd;
	}

	/*** Get commonly used entries and cache them for later access ***/

	/* Hardware version/revision */
	if ( !BD_GetUInt8( &bdCtx, BD_Hw_Ver, 0, &bdHwVer) ) {
		printf("%s() no Hw Version found\n", __FUNCTION__);
	}
	/* Hardware version/revision */
	if ( !BD_GetUInt8( &bdCtx, BD_Hw_Rel, 0, &bdHwRev) ) {
		printf("%s() no Hw Release found\n", __FUNCTION__);
	}
	snprintf(ep->version, sizeof(ep->version), "%d,%d", BD_Hw_Ver, BD_Hw_Rel);

	/* MAC address */
	memset(ep->mac_addr, 0x00, TI_EEPROM_HDR_NO_OF_MAC_ADDR * TI_EEPROM_HDR_ETH_ALEN);
	for (i=0; i<TI_EEPROM_HDR_NO_OF_MAC_ADDR; i++) {
		BD_GetMAC( &bdCtx, BD_Eth_Mac, i, &(ep->mac_addr[i][0]) );
	}

	return 0;

do_fake_bd:
	/* Fill in dummy mac addresses to get u-boot working without valid BD */
	memset(ep->mac_addr, 0x00, TI_EEPROM_HDR_NO_OF_MAC_ADDR * TI_EEPROM_HDR_ETH_ALEN);
	ep->mac_addr[0][5] = 1;
	ep->mac_addr[1][5] = 2;

	return 0;

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
