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
int bd_get_mac(uint index, u8 *mac_address, u32 len);
int bd_get_hw_version(int* pVer, int* pRev);
int bd_get_devicetree(char* devicetreename, size_t len);
int bd_get_context(BD_Context *bdctx, uint32_t i2caddress, uint32_t offset);
void bd_register_context_list(const BD_Context *list, size_t count);
u8 bd_get_boot_partition(void);

#endif	/* __BOARD_DESCRIPTOR_H */
