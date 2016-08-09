#ifndef _BDPARSER_H
#define _BDPARSER_H
/******************************************************************************
 * (c) COPYRIGHT 2009-2011 by NetModule AG, Switzerland.  All rights reserved.
 *
 * The program(s) may only be used and/or copied with the written permission
 * from NetModule AG or in accordance with the terms and conditions stipulated
 * in the agreement contract under which the program(s) have been supplied.
 *
 * PACKAGE : Board descriptor
 *
 * ABSTRACT:
 *  This package implements board descriptor manipulation functions.
 *
 * HISTORY:
 *  Date      Author       Description
 *  20091106  rb           RFE-FB18392: created
 *  20100119  rs           Minor cleanup, tags defined
 *  20100301  rs           Tags redefined
 *  20100302  sma          RFE-FB18392: created (partition)
 *  20100322  th           Adaptation WinCE and Win32 (assert)
 *                         Added get bd info (type and name for standard entries)
 *                         Adjusted bd tags
 *                         Added scan entries (init and get next)
 *                         Added partition info (flags and types)
 *                         Added uint64 and partition64
 *                         Changed boolean value true (BD_TRUE to 1)
 *  20110104  rs           General code cleanup (style guide), added new tags/types
 *                         Added bufLen parameter for BD_GetInfo()
 *                         Fixed wrong sizeof type in GetPartition()
 *                         Changed 64 bit type to "long long" from struct
 *                         Added BD_VerifySha1Hmac() function
 *****************************************************************************/

/**
 * @file
 * Board descriptor parser.
 *  Get() functions are implemented for all supported basis data types:
 *  - 8/16/32 bits unsigned integers
 *  - void
 *  - string
 *  - IPv4 addresses
 *  - Ethernet MAC addresses
 */

/**

\mainpage

\section description    Description

This is the generated documentation for the <b>Board Descriptor utilities</b>.

For more details see the Board Descriptor Design Description.

**/

/*--- component configuration ------------------------------------------------*/

/** Select a target or operating system (just one of course) **/
#undef BD_TARGET_WIN32
#undef BD_TARGET_WINCE
#define BD_TARGET_UBOOT
#undef BD_TARGET_LINUX
#undef BD_TARGET_VXWORKS

#undef BD_CONF_UNIT_TESTS      /**< define this to include unit test functions */

#undef BD_CONF_WANT_ASSERT     /**< define this to use assert functions */

#undef BD_CONF_HAS_HASH        /**< set to include hash check functions in parser */


/** Define external hmac-sha1 function to use */
#ifdef BD_CONF_HAS_HASH
  extern int hmac_sha1(const void* key, unsigned int keylen, const void* data, unsigned int dataLen, void* hash);

  #define BD_SHA1_HASH_FUNC(key, keylen, data, dataLen, hash) \
            hmac_sha1 (key, keylen, data, dataLen, hash)
#endif

/** Define desired assert function */
#ifdef BD_CONF_WANT_ASSERT
  #ifdef BD_TARGET_WINCE
    #define BD_ASSERT(test)  ASSERT(test)
  #elif defined(BD_TARGET_WIN32) && !defined(_DEBUG)
    /* Win32 Release build */
    #include <stdio.h>
    #include <stdlib.h>
    #define BD_ASSERT(test)  { if(!(test)) { printf("BD_ASSERT(%s)\n- file <%s>\n- line <%d>\n", #test, __FILE__, __LINE__ ); exit(1); } }
  #elif defined(BD_TARGET_LINUX)
    #include <linux/kernel.h>
    #define BD_ASSERT(test)  { if(!(test)) { printk(KERN_NOTICE "BD_ASSERT(%s) %s:%d\n", #test, __FILE__, __LINE__ ); } }
  #else
    #include <assert.h>
    #define BD_ASSERT(test)  assert(test)
  #endif
#else
  /* No assertions wanted */
  #define BD_ASSERT(test)  ((void) 0)
#endif /* BD_CONF_WANT_ASSERT */



/*--- defines ----------------------------------------------------------------*/

#define BD_MAX_LENGTH           (4096)    /**< Maximum length of a board descriptor's payload */
#define BD_MAX_ENTRY_LEN        (512)     /**< Maximum length of a tag value */
#define BD_HEADER_LENGTH        (8)       /**< Header is 8 bytes long */
#define BD_MAX_PARTITION_NAME   (16)      /**< Name of partition is at most 16 chars long*/


/*--- types ------------------------------------------------------------------*/

/**
 * Board Descriptor Tags
 */
typedef enum _BD_Tags
{
  BD_End                =     0,   /**<  "Void"       -> End tag */
  BD_Serial             =     1,   /**<  "String"     -> Serial number of the equipment */
  BD_Production_Date    =     2,   /**<  "Date"       -> Production date of the board */
  BD_Hw_Ver             =     3,   /**<  "UInt8"      -> Hardware version of the equipment (Major HW changes, potentionally SW relevant) */
  BD_Hw_Rel             =     4,   /**<  "UInt8"      -> Hardware release of the equipment (Minor HW changes, not SW relevant) */
  BD_Prod_Name          =     5,   /**<  "String"     -> Human readable product name  */
  BD_Prod_Variant       =     6,   /**<  "UInt16"     -> Product variant */
  BD_Prod_Compatibility =     7,   /**<  "String"     -> Product compatibility name */

  BD_Eth_Mac            =     8,   /**<  "MAC"        -> MAC address of the ethernet interface */
  BD_Ip_Addr            =     9,   /**<  "IPV4"       -> IP V4 address (0.0.0.0 = DHCP) */
  BD_Ip_Netmask         =    10,   /**<  "IPV4"       -> IP V4 address mask */
  BD_Ip_Gateway         =    11,   /**<  "IPV4"       -> IP V4 address of the default gateway */

  BD_Usb_Device_Id      =    12,   /**<  "UInt16"      -> USB device ID */
  BD_Usb_Vendor_Id      =    13,   /**<  "UInt16"      -> USB vendor ID */

  BD_Ram_Size           =    14,   /**<  "UInt32"     -> Available RAM size in bytes */
  BD_Ram_Size64         =    15,   /**<  "UInt64"     -> Available RAM size in bytes */
  BD_Flash_Size         =    16,   /**<  "UInt32"     -> Available flash size in bytes */
  BD_Flash_Size64       =    17,   /**<  "UInt64"     -> Available flash size in bytes */
  BD_Eeeprom_Size       =    18,   /**<  "UInt32"     -> Available EEPROM size in bytes */
  BD_Nv_Rram_Size       =    19,   /**<  "UInt32"     -> Available EEPROM size in bytes */

  BD_Cpu_Base_Clk       =    20,   /**<  "UInt32"     -> Base clock of the CPU in Hz = external clock input */
  BD_Cpu_Core_Clk       =    21,   /**<  "UInt32"     -> Core clock of the CPU in Hz */
  BD_Cpu_Bus_Clk        =    22,   /**<  "UInt32"     -> Bus clock of the CPU in Hz */
  BD_Ram_Clk            =    23,   /**<  "UInt32"     -> RAM clock in Hz */

  BD_Partition          =    24,   /**<  "Partition"   -> Offset of 1st Uboot partition in the 1st flash device in bytes */
  BD_Partition64        =    25,   /**<  "Partition64" -> Offset of 1st Uboot partition in the 1st flash device in bytes */

  BD_Lcd_Type           =    26,   /**<  "UInt16"     -> LCD type -> 0 = not present (interpretation can be project specific) */
  BD_Lcd_Backlight      =    27,   /**<  "UInt8"      -> LCD backlight setting (0 = off; 100=max) */
  BD_Lcd_Contrast       =    28,   /**<  "UInt8"      -> LCD contrast setting (0 = min; 100=max) */
  BD_Touch_Type         =    29,   /**<  "UInt16"     -> Touch Screen type --> 0 = not present/defined */

  BD_Manufacturer_Id    =    30,   /**<  "String"     -> Manufacturer id of the produced equipment (i.e. barcode) */
  BD_Hmac_Sha1_4        =    31,   /**<  "Hash"       -> SHA1 HMAC with 4 byte result */
  BD_Fpga_Info          =    32,   /**<  "UInt32"     -> FPGA type/location (0xTTPPRRRR TT=FPGA type, PP=Population location, RRRR=Reserved allways 0000) */

  BD_Ui_Adapter_Type    =  4096,   /**<  "UInt16"     -> IV OG2 UI adapterboard type (0 = not present) */

  /* project specific tags */
  BD_BootPart			= 32768,   /**<  "UInt8" */

  BD_None_Type          = 65535,   /**<  "Void"       -> None */
}
BD_Tags;

/**
 * Board Descriptor Tag Types
 */
typedef enum _BD_Type
{
  BD_Type_End         = 0x00000000,
  BD_Type_Void        = 0x00000001,
  BD_Type_UInt8       = 0x00000002,
  BD_Type_UInt16      = 0x00000003,
  BD_Type_UInt32      = 0x00000004,
  BD_Type_UInt64      = 0x00000005,
  BD_Type_String      = 0x00000010,
  BD_Type_Date        = 0x00000020,
  BD_Type_MAC         = 0x00000030,
  BD_Type_IPV4        = 0x00000040,
  BD_Type_Partition   = 0x00000050,
  BD_Type_Partition64 = 0x00000051,
  BD_Type_HMAC        = 0x00000060,
  BD_Type_None        = 0xFFFFFFFF,
}
BD_Type;


typedef unsigned int    bd_uint_t;      /**< Generic UInt */
typedef unsigned int    bd_size_t;      /**< Size type */

typedef unsigned char   bd_uint8_t;     /**< 8 Bit unsigned integer */
typedef unsigned short  bd_uint16_t;    /**< 16 Bit unsigned integer */
typedef unsigned int    bd_uint32_t;    /**< 32 Bit unsigned integer */

#if defined(BD_TARGET_WIN32) || defined (BD_TARGET_WINCE)
  typedef unsigned __int64 bd_uint64_t;   /**< 64 Bit unsigned integer */
#else
  typedef unsigned long long bd_uint64_t; /**< 64 Bit unsigned integer */
#endif

typedef int             bd_bool_t;      /**< Boolean */
#define BD_FALSE        0               /**< Boolean FALSE */
#define BD_TRUE         1               /**< Boolean  TRUE */

typedef struct _BD_Info
{
  BD_Tags           tag;
  BD_Type           type;
  const char*       pName;
}
BD_Info;

typedef struct _BD_Entry
{
  bd_uint16_t       tag;           /**< Tag of entry */
  bd_size_t         len;           /**< Length of entry */
  bd_uint_t         entry;         /**< Number of entry */
  const bd_uint8_t* pData;         /**< Pointer to descriptor data of entry */
}
BD_Entry;


/**
 * Board Descriptor Context
 *
 * This structure is passed to all calls of a Board Descriptor function.
 * It stores the required context information.
 * The entries are solely used by the Board Descriptor functions.
 * They must not be accessed by the user.
 */
typedef struct _BD_Context
{
  bd_bool_t          headerOk;      /**< True if header check passed else false */
  bd_bool_t          initialized;   /**< True if data imported (and checked) */

  bd_uint_t          size;          /**< Size of descriptor data */
  bd_uint_t          entries;       /**< Number of entries found */

  bd_uint16_t        checksum;      /**< Payload checksum contained in the header */
  const bd_uint8_t*  pData;         /**< Pointer to descriptor data (not header) */
  const bd_uint8_t*  pDataEnd;      /**< Pointer to end of data */
}
BD_Context;


/*
 * Partition Flags
 */
typedef enum _BD_Partition_Flags
{
  BD_Partition_Flags_None           = 0x00,     /**< No special flags */
  BD_Partition_Flags_Active         = 0x80,     /**< Partition is active */
}
BD_Partition_Flags;

/*
 * Partition Type
 */
typedef enum _BD_Partition_Type
{
  BD_Partition_Type_Raw             = 0,        /**< Unspecified type */
  BD_Partition_Type_Raw_BootLoader  = 1,        /**< Linear bootloader image */
  BD_Partition_Type_Raw_BBT         = 2,        /**< Bad Block Table */
  BD_Partition_Type_FS_YAFFS2       = 3,        /**< YAFFS2 Partition */
  BD_Partition_Type_FS_JFFS2        = 4,        /**< JFFS2 Partition */
  BD_Partition_Type_FS_FAT16        = 5,        /**< FAT16 Partition */
  BD_Partition_Type_FS_FAT32        = 6,        /**< FAT32 Partition */
  BD_Partition_Type_FS_EXFAT        = 7,        /**< EXFAT Partition */

  BD_Partition_Type_Max             = 8,        /**< For error checks */
}
BD_Partition_Type;

/*
 * Partition Options (Partition64 element only)
 */
typedef enum _BD_Partition_Options
{
  BD_Partition_Opts_None            = 0x00,     /***< No special options */
  BD_Partition_Opts_ReadOnly        = 0x01,     /***< Partition should be mounted read only */
  BD_Partition_Opts_OS              = 0x02,     /***< Partition contains operating system (OS) */
}
BD_Partition_Options;

/**
 * Board descriptor type to describe filesystem partitions
 *
 * The function BD_GetPartition will directly fill such a structure.
 */
typedef struct _BD_PartitionEntry
{
  BD_Partition_Flags    flags;
  BD_Partition_Type     type;
  bd_uint32_t           offset;
  bd_uint32_t           size;
  char                  name[BD_MAX_PARTITION_NAME+1];
}
BD_PartitionEntry;

/**
 * Board descriptor type to describe filesystem partitions
 *
 * Extended version with 64 bit addresses and options field.
 * The function BD_GetPartition64 will directly fill such a structure.
 */
typedef struct _BD_PartitionEntry64
{
  BD_Partition_Flags    flags;
  BD_Partition_Type     type;
  BD_Partition_Options  options;
  bd_uint64_t           offset;
  bd_uint64_t           size;
  char                  name[BD_MAX_PARTITION_NAME+1];
}
BD_PartitionEntry64;


/*--- function prototypes ----------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Checks a BD header's validity and updates the BD context.
 *
 * @param[in,out] pCtx      The context of the BD being checked.
 * @param[in]     pHeader   Pointer to the BD header
 * @return        True if the header is valid and the context was updated.
 *                False if the header s not valid.
 */
bd_bool_t BD_CheckHeader( BD_Context* pCtx, const void* pHeader );

/**
 * Imports BD data from a buffer into a BD context.
 *
 * @param[in,out] pCtx      The context into which data is imported.
 * @param[in]     pData     Pointer to the buffer containing the BD entries.
 * @return        True if BD entries could be succesfuly imported.
 *                False if there is an error in the buffer data structure.
 */
bd_bool_t BD_ImportData( BD_Context* pCtx, const void* pData );

/**
 * Checks the existence of a tag in the BD
 *
 * @param[in,out] pCtx      The context in which the tag is searched.
 * @param[in]     tag       Tag being checked.
 * @param[in]     index     Index of the tag (0=first index).
 * @return        True if the entry exists in the BD else False.
 */
bd_bool_t BD_ExistsEntry( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index );

/**
 * Get type and name of a tag in the BD info table
 *
 * @param[in]     tag       Tag reference.
 * @param[out]    pType     Type of the tag (0 if not used).
 * @param[out]    pName     Name of the tag (0 if not used).
 * @param[in]     bufLen    Length of the pName buffer.
 *                          If required the returned string for pName will be truncated.
 * @return        True if the tag in the BD info table exists else False.
 */
bd_bool_t BD_GetInfo( bd_uint16_t tag, BD_Type* pType, char* pName, bd_size_t bufLen );

/**
 * Initialize the entry before use BD_GetNextEntry
 *
 * @param[out]    pEntry    BD entry to be initalized.
 * @return        True if the entry was initialized, fasle otherwise.
 */
bd_bool_t BD_InitEntry( BD_Entry* pEntry);

/**
 * Get type and name of a tag in the BD info table
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[out]    pEntry    BD entry (use BD_InitEntry, not 0 for first ).
 * @return        True if the tag in the BD info table exists else False.
 */
bd_bool_t BD_GetNextEntry( const BD_Context* pCtx, BD_Entry* pEntry );


/**
 * Gets a void value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   True if the value could be found else False.
 * @return        False if something went wrong dring the parsing else True.
 */
bd_bool_t BD_GetVoid( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_bool_t* pResult );

/**
 * Gets an 8 bits unsigned integer value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetUInt8( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint8_t* pResult );

/**
 * Gets a 16 bits unsigned integer value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetUInt16( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint16_t* pResult );

/**
 * Gets a 32 bits unsigned integer value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetUInt32( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint32_t* pResult );

/**
 * Gets a 64 bits unsigned integer value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetUInt64( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint64_t* pResult );

/**
 * Gets a string value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @param[in]     bufLen    Length of the pResult buffer.
 * @return        True if the value in pResult is valid else False.
 *
 * @note @li The returned string in pResult is null-terminated.
 *       @li If the buffer is too.small to hold the value the returned string is truncated.
 */
bd_bool_t BD_GetString( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, char* pResult, bd_size_t bufLen );

/**
 * Gets a binary large object (blob) value from a BD.
 *
 * @param[in]     pCtx      The context from which the value is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read value.
 * @param[in]     bufLen    Length of the pResult buffer.
 * @param[out]    pReadLen  The actual number of bytes read.
 * @return        True if the complete tag value could be read in pResult else False.
 */
bd_bool_t BD_GetBlob( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index,
                      char* pResult, bd_size_t bufLen, bd_size_t* pReadLen );

/**
 * Gets an IPv4 address from a BD.
 *
 * The IP address is returned as a 32 bits unsigned integer with the most
 * significant byte first. E.g. 192.168.2.1 is stored as 0xC0A80201
 *
 * @param[in]     pCtx      The context from which the IP address is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read IP address.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetIPv4( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint32_t* pResult );

/**
 * Gets an Ethernet MAC address from a BD.
 *
 * @param[in]     pCtx      The context from which the MAC address is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the read MAC address.
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetMAC( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, bd_uint8_t pResult[6] );

/**
 * Gets a partition entry from a BD.
 *
 * @param[in,out] pCtx      The context from which the MAC address is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the partition entry
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetPartition( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, BD_PartitionEntry* pResult );

/**
 * Gets a partition64 entry from a BD.
 *
 * @param[in,out] pCtx      The context from which the MAC address is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[out]    pResult   Placeholder for the partition entry
 * @return        True if the value in pResult is valid else False.
 */
bd_bool_t BD_GetPartition64( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, BD_PartitionEntry64* pResult );


#ifdef BD_CONF_HAS_HASH

/**
 * Verifies the SHA1-HMAC checksum.
 *
 * The checksum is computed with the specified key over the area defined
 * by the hash tag. The key must match the one used to generate the descriptor.
 *
 * @param[in]     pCtx      The context from which the MAC address is read.
 * @param[in]     tag       Tag Id.
 * @param[in]     index     Index of the tag (0=first occurance).
 * @param[in]     pKey      Pointer to key for HMAC initialization.
 * @param[in]     keyLen    Size of the key.
 * @return        True if the protected data is unmodified. False in any other case.
 */
bd_bool_t BD_VerifySha1Hmac( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, const void* pKey, bd_size_t keyLen );

#endif


#ifdef BD_CONF_UNIT_TESTS

/**
 * Runs unit tests
 *
 * If an error occurs an assert is triggered
 */
void BD_UnitTest(void);

#endif /* BD_CONF_UNIT_TESTS */


#ifdef __cplusplus
} /*end extern c*/
#endif

#endif /* _BDPARSER_H */

