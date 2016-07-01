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
#include <malloc.h>

#include "board_detect.h"
#include "bdparser.h"

#define SYSINFO_ADDRESS					0x0000  /* Board descriptor at beginning of EEPROM */
#define SYSCONFIG_ADDRESS				0x0600  /* Board descriptor at beginning of EEPROM */
#define MAX_PARTITION_ENTRIES			4

static struct ti_common_eeprom bd_mirror;

static BD_Context *bd_board_info = 0;
static BD_Context *bd_system_config = 0;

static u8 boot_partition = 0;

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

	// TODO read from real eeprom
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

void read_sysinfo(void)
{
	u8          bdHwVer     = 0;
	u8          bdHwRev     = 0;
	int			err;
	int			i;
	int			j;

	err = boardinfo_read(&bd_board_info, SYSINFO_ADDRESS);
	if (err ) {
		printf("Could not read sysinf boarddescriptor\n");
		goto do_fake_bd;
	}

	/* Hardware version/revision */
	if ( !BD_GetUInt8( bd_board_info, BD_Hw_Ver, 0, &bdHwVer) ) {
		printf("%s() no Hw Version found\n", __FUNCTION__);
	}
	/* Hardware version/revision */
	if ( !BD_GetUInt8( bd_board_info, BD_Hw_Rel, 0, &bdHwRev) ) {
		printf("%s() no Hw Release found\n", __FUNCTION__);
	}
	snprintf(bd_mirror.version, sizeof(bd_mirror.version), "%d,%d", bdHwVer, bdHwRev);

	/* MAC address */
	memset(bd_mirror.mac_addr, 0x00, TI_EEPROM_HDR_NO_OF_MAC_ADDR * TI_EEPROM_HDR_ETH_ALEN);
	for (i=0; i<TI_EEPROM_HDR_NO_OF_MAC_ADDR; i++) {
		u8 mac[6];
		BD_GetMAC( bd_board_info, BD_Eth_Mac, i, mac);
		/* Convert nm MAC to TI MAC */
		for (j=0; j<6; j++){
			bd_mirror.mac_addr[i][j] = mac[j];
		}
	}

	return;

do_fake_bd:
	printf("%s() do fake boarddescriptor\n", __FUNCTION__);
	/* Fill in dummy mac addresses to get u-boot working without valid BD */
	memset(bd_mirror.mac_addr, 0x00, TI_EEPROM_HDR_NO_OF_MAC_ADDR * TI_EEPROM_HDR_ETH_ALEN);
	bd_mirror.mac_addr[0][5] = 1;
	bd_mirror.mac_addr[1][5] = 2;
}

void try_partition_read(void)
{
	BD_PartitionEntry64 partition;
	int i;
	int rc;
	int partition_count = 0;

	for (i = 0; i < MAX_PARTITION_ENTRIES; i++)
	{
		rc = BD_GetPartition64( bd_system_config, BD_Partition64, i, &partition );
		if (rc) {
			partition_count++;
			if (((partition.flags & BD_Partition_Flags_Active) != 0) &&
					(i > 0)) {
				boot_partition = i - 1; /* The first one is a dummy partition for u-boot */
			}
		}
	}

	if (partition_count < 1)
	{
		printf("ERROR: Too few partitions defined\n");
	}

	printf("Found %d partitions\n", partition_count);
}

void read_sysconfig(void)
{
	int err;
	u8 boot_part;

	err = boardinfo_read(&bd_system_config, SYSCONFIG_ADDRESS);
	if (err ) {
		printf("Could not read sysconfig boarddescriptor\n");
	}

	/* If we have a new Bootpartition entry take this as boot part */
	if ( BD_GetUInt8( bd_system_config, BD_BootPart, 0, &boot_part) ) {
		if (boot_part >= 0 && boot_part <= 1) {
			boot_partition = boot_part;
			return;
		}
	}

	/* If we not have a Bootpartition entry, perhaps we have a partition table */
	try_partition_read();
}

int __maybe_unused ti_i2c_eeprom_am_get(int bus_addr, int dev_addr)
{
	if (bd_mirror.header == TI_EEPROM_HEADER_MAGIC)
		return 0;

	read_sysinfo();
	read_sysconfig();

	bd_mirror.header = TI_EEPROM_HEADER_MAGIC;

	return 0;
}

bool __maybe_unused board_ti_is(char *name_tag)
{
	if (bd_mirror.header == TI_DEAD_EEPROM_MAGIC)
		return false;
	return !strncmp(bd_mirror.name, name_tag, TI_EEPROM_HDR_NAME_LEN);
}

char * __maybe_unused board_ti_get_rev(void)
{
	if (bd_mirror.header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return bd_mirror.version;
}

char * __maybe_unused board_ti_get_config(void)
{
	if (bd_mirror.header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return bd_mirror.config;
}

char * __maybe_unused board_ti_get_name(void)
{
	if (bd_mirror.header == TI_DEAD_EEPROM_MAGIC)
		return NULL;

	return bd_mirror.name;
}

void __maybe_unused set_board_info_env(char *name)
{
	return;
}

void __maybe_unused
board_ti_get_eth_mac_addr(int index,
			  u8 mac_addr[TI_EEPROM_HDR_ETH_ALEN])
{
	if (bd_mirror.header == TI_DEAD_EEPROM_MAGIC)
		goto fail;

	if (index < 0 || index >= TI_EEPROM_HDR_NO_OF_MAC_ADDR)
		goto fail;

	memcpy(mac_addr, bd_mirror.mac_addr[index], TI_EEPROM_HDR_ETH_ALEN);
	return;

fail:
	memset(mac_addr, 0, TI_EEPROM_HDR_ETH_ALEN);
}

u8 get_boot_partition(void)
{
	return boot_partition;
}
