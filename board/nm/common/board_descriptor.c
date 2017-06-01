/******************************************************************************
 * (c) COPYRIGHT 2010 by NetModule AG, Switzerland.  All rights reserved.
 *
 * The program(s) may only be used and/or copied with the written permission
 * from NetModule AG or in accordance with the terms and conditions stipulated
 * in the agreement contract under which the program(s) have been supplied.
 *
 * PACKAGE : NetBox HW08
 *
 * ABSTRACT:
 *  Implements functions for settings
 *
 * HISTORY:
 *  Date      Author       Description
 *  20100421  SMA          created
 *  20100903  rs           reading carrier board descriptor from EEPROM at 54.
 *                         code cleanup (tabs/indentation)
 *  20110211  rs           partition table handling
 *
 *****************************************************************************/
#include <common.h>
#include <i2c.h>
#include <malloc.h>

#include "bdparser.h"	   /* tlv parser */

#define MAX_PARTITION_ENTRIES			4

static const BD_Context		*bdctx_list;		/* The board descriptor context */
static size_t				bdctx_count = 0;

void bd_register_context_list(const BD_Context *list, size_t count) {
	bdctx_list = list;
	bdctx_count = count;
}

int bd_get_context(BD_Context *bdctx, uint32_t i2caddress, uint32_t offset)
{
	bd_bool_t   rc;
	uint8_t		bdHeader[8];
	void*		pBdData = NULL;
	/* Read header bytes from beginning of EEPROM */
	if (i2c_read( i2caddress, offset, 2, bdHeader, BD_HEADER_LENGTH )) {
		debug("%s() Can't read BD header from EEPROM\n", __func__);
		goto exit1;
	}

	/* Check whether this is a valid board descriptor (or empty EEPROM) */
	rc = BD_CheckHeader( bdctx, bdHeader );
	if (!rc) {
		debug("%s() No valid board descriptor found\n", __func__);
		goto exit1;
	}

	/* Allocate memory for descriptor data and .. */
	pBdData = malloc( bdctx->size );
	if ( pBdData == NULL ) {
		debug("%s() Can't allocate %d bytes\n", __func__, bdctx->size);
		goto exit1;
	}

	/* .. read data from EEPROM */
	if (i2c_read(i2caddress, offset+BD_HEADER_LENGTH, 2, pBdData, bdctx->size)) {
		debug("%s() Can't read data from EEPROM\n", __func__);
		goto exit1;
	}

	/*
	 * Import data into board descriptor context
	 */
	rc = BD_ImportData( bdctx, pBdData );
	if (!rc) {
		debug("%s() Invalid board descriptor data\n", __func__);
		goto exit1;
	}

	return 0;

exit1:
	if (pBdData != NULL)
	{
		free(pBdData);
		pBdData = NULL;
	}

	return -1;
}

static bd_bool_t _get_string(bd_uint16_t tag, bd_uint_t index, char* pResult, bd_size_t bufLen ) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetString(&bdctx_list[i], tag, index, pResult, bufLen)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

static bd_bool_t _get_mac( bd_uint16_t tag, bd_uint_t index, bd_uint8_t pResult[6] ) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetMAC(&bdctx_list[i], tag, index, pResult)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

static bd_bool_t _get_uint8( bd_uint16_t tag, bd_uint_t index, bd_uint8_t* pResult ) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetUInt8(&bdctx_list[i], tag, index, pResult)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

static bd_bool_t _get_uint16( bd_uint16_t tag, bd_uint_t index, bd_uint16_t* pResult ) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetUInt16(&bdctx_list[i], tag, index, pResult)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

static bd_bool_t _get_uint32( bd_uint16_t tag, bd_uint_t index, bd_uint32_t* pResult ) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetUInt32(&bdctx_list[i], tag, index, pResult)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

static bd_bool_t _get_partition64( bd_uint16_t tag, bd_uint_t index, BD_PartitionEntry64 *pResult) {
	int i;

	for (i = 0; i < bdctx_count; i++) {
		if (BD_GetPartition64(&bdctx_list[i], tag, index, pResult)) {
			return BD_TRUE;
		}
	}

	return BD_FALSE;
}

int bd_get_prodname(char *prodname, size_t len)
{
	if ( !_get_string( BD_Prod_Name, 0, prodname, len) ) {
		debug("%s() Product name not found\n", __func__);
		return -1;
	}

	return 0;
}

void bd_get_hw_version(int* ver, int* rev)
{
	static uint8_t hwver;
	static uint8_t hwrev;

	if ( !_get_uint8( BD_Hw_Ver, 0, &hwver) )
		debug("%s() no Hw Version found\n", __func__);

	if ( !_get_uint8( BD_Hw_Rel, 0, &hwrev) )
		debug("%s() no Hw Release found\n", __func__);

	*ver = hwver;
	*rev = hwrev;
}

int bd_get_mac(int index, uint8_t *macaddr, size_t len)
{
	if (len != 6) {
		debug("macaddr size must be 6 (is %d)", len);
		return -1;
	}

	/* MAC address */
	if ( !_get_mac( BD_Eth_Mac, index, macaddr) ) {
		debug("%s() MAC addresss %d not found\n", __func__, index);
		return -1;
	}

	return 0;
}

u32 bd_get_fpgainfo(void)
{
	uint32_t fpgainfo = 0xFFFFFFFF;

	if ( !_get_uint32( BD_Fpga_Info, 0, &fpgainfo) )
		debug("%s() no Fpga Info found\n", __func__);

	return fpgainfo;
}

int bd_get_pd_module(int slot, char *config, size_t len)
{
	if ( !_get_string(BD_Pd_Module0 + slot, 0, config, len) ) {
		debug("%s() could not read module configuration on slot %d\n",
				__func__, slot);
		return -1;
	}

	return 0;
}

int bd_get_sim_config(char* simconfig, size_t len)
{
	if (!_get_string(BD_Pd_Sim, 0, simconfig, len)) {
		debug("%s() No valid SIM Config found\n", __func__);
		return -1;
	}

	return 0;
}

int bd_get_devicetree(char* devicetreename, size_t len)
{
	if (!_get_string(PD_Dev_Tree, 0, devicetreename, len)) {
		debug("%s() No valid Devicetree name found\n", __func__);
		return -1;
	}

	return 0;
}

int bd_get_shield(int shieldnr)
{
    bd_uint16_t shield = 0;

	if (!_get_uint16(PD_Shield, shieldnr, &shield) ) {
		debug("%s() no shield populated\n", __func__);
        return -1;
    }

	return shield;
}

static u8 try_partition_read(void)
{
	BD_PartitionEntry64 partition;
	int i;
	int rc;
	int partition_count = 0;
	int boot_partition = 0;

	for (i = 0; i < MAX_PARTITION_ENTRIES; i++)
	{
		rc = _get_partition64(BD_Partition64, i, &partition);
		if (rc) {
			partition_count++;
			if (((partition.flags & BD_Partition_Flags_Active) != 0) &&
					(i > 0)) {
				boot_partition = i;
			}
		}
	}

	if (partition_count < 1)
	{
		printf("ERROR: Too few partitions defined, taking default 0\n");
	}

	return boot_partition;

}

int bd_get_boot_partition(void)
{
	u8 boot_part;

	/* If we have a new Bootpartition entry take this as boot part */
	if ( _get_uint8( BD_BootPart, 0, &boot_part) ) {
		if (boot_part >= 0 && boot_part <= 1) {
			return boot_part;
		}
	}

	/* If we not have a Bootpartition entry, perhaps we have a partition table */
	return try_partition_read();
}
