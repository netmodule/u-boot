/*
 * Library to support early TI EVM EEPROM handling
 *
 * Copyright (C) 2015-2016 Texas Instruments Incorporated - http://www.ti.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __BOARD_DESCRIPTOR_H
#define __BOARD_DESCRIPTOR_H

int bd_read(int bus_addr, int dev_addr);
u8 bd_get_boot_partition(void);
int bd_get_mac_address(uint index, u8 *mac_address, u32 len);
int bd_get_hw_version(int* pVer, int* pRev);

#endif	/* __BOARD_DESCRIPTOR_H */
