/*
 * Library to support early TI EVM EEPROM handling
 *
 * Copyright (C) 2015-2016 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla
 *	Steve Kipisz
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/omap_common.h>
#include <i2c.h>
#include <malloc.h>
#include <asm/io.h>

#include "board_descriptor.h"
#include "bdparser.h"

#define SYSINFO_ADDRESS					0x0000  /* Board descriptor at beginning of EEPROM */
#define SYSCONFIG_ADDRESS				0x0600  /* Board descriptor at beginning of EEPROM */
#define MAX_PARTITION_ENTRIES			4

static BD_Context *bd_board_info = 0;
static BD_Context *bd_system_config = 0;

static int i2c_eeprom_init(int i2c_bus, int dev_addr)
{
	int rc;

	if (i2c_bus >= 0) {
		rc = i2c_set_bus_num(i2c_bus);
		if (rc)
			return rc;
	}

	return i2c_probe(dev_addr);
}

static int i2c_eeprom_read(int offset, void *data, size_t len)
{
	return i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR,
		offset,
		CONFIG_SYS_I2C_EEPROM_ADDR_LEN,
		data,
		len);
}

static int boardinfo_read(BD_Context **context, size_t start_addr)
{
	char bd_header_buffer[8];
	void *bd_data = NULL;

	if(*context)
		return 0;

	*context = calloc(sizeof(BD_Context), 1);
	if(!*context)
	{
		printf("Couldn't allocate memory for board information\n");
		goto failed;
	}

	if (i2c_eeprom_read(start_addr, bd_header_buffer, sizeof(bd_header_buffer))) {
		printf("%s() Can't read BD header from EEPROM\n", __FUNCTION__);
		goto failed;
	}

	if (!BD_CheckHeader(*context, bd_header_buffer))
	{
		printf("Invalid board information header\n");
		goto failed;
	}

	bd_data = malloc((*context)->size);
	if (bd_data == NULL)
	{
		printf("Can not allocate memory for board info");
		goto failed;
	}

	if (i2c_eeprom_read(start_addr + sizeof(bd_header_buffer), bd_data, (*context)->size))
	{
		printf("Can not read board information data");
		goto failed;
	}

	if (!BD_ImportData(*context, bd_data))
	{
		printf("Invalid board information!\n");
		goto failed;
	}

	return 0;

failed:
	if (bd_data != NULL)
	{
		free(bd_data);
		bd_data = NULL;
	}

	if (*context != NULL)
	{
		free(*context);
		*context = NULL;
	}

	return -1;
}

static void read_sysinfo(void)
{
	int			err;

	err = boardinfo_read(&bd_board_info, SYSINFO_ADDRESS);
	if (err ) {
		printf("Could not read sysinf boarddescriptor\n");
		return;
	}

	return;
}

static void read_sysconfig(void)
{
	int err;

	err = boardinfo_read(&bd_system_config, SYSCONFIG_ADDRESS);
	if (err ) {
		printf("Could not read sysconfig boarddescriptor\n");
		return;
	}
}

int bd_read (int bus_addr, int dev_addr)
{
	if (i2c_eeprom_init(bus_addr, dev_addr)) {
		return -1;
	}

	read_sysinfo();
	read_sysconfig();

	return 0;
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
		rc = BD_GetPartition64( bd_system_config, BD_Partition64, i, &partition );
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

u8 bd_get_boot_partition(void)
{
	u8 boot_part;

	if ((bd_system_config == 0)) {
		puts("System config not valid, can not get boot partition\n");
		return 0;
	}

	/* If we have a new Bootpartition entry take this as boot part */
	if ( BD_GetUInt8( bd_system_config, BD_BootPart, 0, &boot_part) ) {
		if (boot_part >= 0 && boot_part <= 1) {
			return boot_part;
		}
	}

	/* If we not have a Bootpartition entry, perhaps we have a partition table */
	return try_partition_read();

}

int bd_get_mac_address(uint index, u8 *mac, u32 len)
{
	if (bd_board_info == 0) {
		puts("Board info not valid, can not get mac address\n");
		return -1;
	}

	if (len != 6) {
		return -1;
	}

	if (BD_GetMAC( bd_board_info, BD_Eth_Mac, index, mac))
		return 0;
	else
		return -1;
}

int bd_get_hw_version(int* pVer, int* pRev)
{
	u8 bdCpHwVer = 0;
	u8 bdCpHwRev = 0;

	if (bd_board_info == 0) {
		puts("Board info not valid, can not get hw version\n");
		return -1;
	}
	/* Hardware version/revision */
	if ( !BD_GetUInt8( bd_board_info, BD_Hw_Ver, 0, &bdCpHwVer) ) {
		printf("no Hw version found\n");
		return -1;
	}
	/* Hardware version/revision */
	if ( !BD_GetUInt8( bd_board_info, BD_Hw_Rel, 0, &bdCpHwRev) ) {
		printf("no Hw release found\n");
		return -1;
	}

	*pVer = bdCpHwVer;
	*pRev = bdCpHwRev;

	return 0;
}

