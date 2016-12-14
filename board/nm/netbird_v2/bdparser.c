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
 *  20091110  rb           Added checksum in header
 *  20100121  rs           Minor cleanup, const arguments specified
 *  20100302  sma          RFE-FB18392: created (partition)
 *  20100322  th           Adaptation WinCE and Win32 (includes/warnings)
 *                         Added get bd info (type and name for standard entries)
 *                         Added scan entries (init and get next)
 *                         Added test scan entries (get entry)
 *                         Added uint64 and partition64
 *  20110104  rs           General code cleanup (style guide), added new tags/types
 *                         Added bufLen parameter for BD_GetInfo()
 *                         Fixed wrong sizeof type in GetPartition()
 *                         Changed 64 bit type to "long long" from struct
 *                         Added BD_VerifySha1Hmac() function
 *****************************************************************************/

/*--- includes --------------------------------------------------------------*/

#include "bdparser.h"             /* own header file */
#include <asm/io.h>


/* Platform specific include files */

#ifdef BD_TARGET_WIN32
  #include <string.h>         /* memcpy, strlen, memset, memcmp */
#endif

#ifdef BD_TARGET_WINCE
  #pragma warning(push)
  #pragma warning(disable: 4115 4201 4204 4214)
  #include <windows.h>
  #pragma warning(pop)
#endif

#ifdef BD_TARGET_LINUX
  #include <linux/string.h>   /* memcpy, strlen, memset, memcmp */
#endif

#ifdef BD_TARGET_UBOOT
  #include <common.h>         /* memcpy, etc. */
#endif

#ifdef BD_TARGET_VXWORKS
  #include "string.h"
//  #include "stdlib.h"
//  #include "stdio.h"
//  #include "ioLib.h"
//  #include "vxWorks.h"
#endif

/* Make sure asserts are enabled when unit tests are required */
#if defined(BD_CONF_UNIT_TESTS) && !defined (BD_CONF_WANT_ASSERT)
  #error BD_CONF_UNIT_TESTS requires assertions
#endif


/*--- local variables -------------------------------------------------------*/

static const BD_Info bd_info[] = {
  { BD_Serial             , BD_Type_String     , "serial"             },
  { BD_Production_Date    , BD_Type_Date       , "proddate"           },
  { BD_Hw_Ver             , BD_Type_UInt8      , "hwver"              },
  { BD_Hw_Rel             , BD_Type_UInt8      , "hwrel"              },
  { BD_Prod_Name          , BD_Type_String     , "prod_name"          },
  { BD_Prod_Variant       , BD_Type_UInt16     , "prod_variant"       },
  { BD_Prod_Compatibility , BD_Type_String     , "prod_compatibility" },

  { BD_Eth_Mac            , BD_Type_MAC        , "eth_mac"            },
  { BD_Ip_Addr            , BD_Type_IPV4       , "ip_addr"            },
  { BD_Ip_Netmask         , BD_Type_IPV4       , "ip_netmask"         },
  { BD_Ip_Gateway         , BD_Type_IPV4       , "ip_gateway"         },

  { BD_Usb_Device_Id      , BD_Type_UInt16     , "usb_device_id"      },
  { BD_Usb_Vendor_Id      , BD_Type_UInt16     , "usb_vendor_id"      },

  { BD_Ram_Size           , BD_Type_UInt32     , "ram_size"           },
  { BD_Ram_Size64         , BD_Type_UInt64     , "ram_size64"         },
  { BD_Flash_Size         , BD_Type_UInt32     , "flash_size"         },
  { BD_Flash_Size64       , BD_Type_UInt64     , "flash_size64"       },
  { BD_Eeeprom_Size       , BD_Type_UInt32     , "eeprom_size"        },
  { BD_Nv_Rram_Size       , BD_Type_UInt32     , "nv_ram_size"        },

  { BD_Cpu_Base_Clk       , BD_Type_UInt32     , "cpu_base_clk"       },
  { BD_Cpu_Core_Clk       , BD_Type_UInt32     , "cpu_core_clk"       },
  { BD_Cpu_Bus_Clk        , BD_Type_UInt32     , "cpu_bus_clk"        },
  { BD_Ram_Clk            , BD_Type_UInt32     , "ram_clk"            },

  { BD_Partition          , BD_Type_Partition  , "partition"          },
  { BD_Partition64        , BD_Type_Partition64, "partition64"        },

  { BD_Lcd_Type           , BD_Type_UInt16     , "lcd_type"           },
  { BD_Lcd_Backlight      , BD_Type_UInt8      , "lcd_backlight"      },
  { BD_Lcd_Contrast       , BD_Type_UInt8      , "lcd_contrast"       },
  { BD_Touch_Type         , BD_Type_UInt16     , "touch_type"         },

  { BD_Manufacturer_Id    , BD_Type_String     , "manufacturer_id"    },
  { BD_Hmac_Sha1_4        , BD_Type_HMAC       , "hmac-sha1"          },

  { BD_Ui_Adapter_Type    , BD_Type_UInt16     , "ui_adapter_type"    },

  /* Guard entry, must be last in array (don't remove) */
  { BD_End                , BD_Type_End        , 0                    },
};


/*--- local functions -------------------------------------------------------*/

static void bd_safeStrCpy( char* pDest, bd_size_t destLen,
                           const char* pSrc, bd_size_t srcLen )
{
  bd_size_t   bytesToCopy = 0;

  /* Argument check */

  BD_ASSERT( pDest != 0 );
  BD_ASSERT( destLen >= 1 );
  BD_ASSERT( pSrc != 0 );
  BD_ASSERT( srcLen <= BD_MAX_ENTRY_LEN );

  /* Copy string, truncate if necessary */
  if ( srcLen <= (destLen - 1) )
  {
    /* Whole string fits into supplied buffer */
    bytesToCopy = srcLen;
  }
  else
  {
    /* Truncate string to buffer size */
    bytesToCopy = destLen - 1;
  }

  if ( bytesToCopy > 0 )
  {
    memcpy( pDest, pSrc, bytesToCopy );
  }
  pDest[bytesToCopy] = 0;
}

/*---------------------------------------------------------------------------*/

static bd_uint16_t bd_getUInt16( const void* pBuf )
{
  const bd_uint8_t*  pui8Buf = 0;
  bd_uint16_t        value;

  BD_ASSERT( pBuf != 0 );

  pui8Buf = (const bd_uint8_t*)pBuf;
  value =   (pui8Buf[0] <<  8)
          | (pui8Buf[1] <<  0);

  return value;
}

/*---------------------------------------------------------------------------*/

static bd_uint32_t bd_getUInt32( const void* pBuf )
{
  const bd_uint8_t*  pui8Buf = 0;
  bd_uint32_t        value;

  BD_ASSERT( pBuf != 0 );

  pui8Buf = (const bd_uint8_t*)pBuf;
  value =   (pui8Buf[0] << 24)
          | (pui8Buf[1] << 16)
          | (pui8Buf[2] <<  8)
          | (pui8Buf[3] <<  0);

  return value;
}

/*---------------------------------------------------------------------------*/

static bd_uint64_t bd_getUInt64( const void* pBuf )
{
  const bd_uint8_t*  pui8Buf = 0;
  bd_uint64_t        value;
  int                i;

  BD_ASSERT( pBuf != 0 );
  pui8Buf = (const bd_uint8_t*)pBuf;

  value = 0;
  for (i=0; i<8; i++)
  {
    value <<= 8;
    value |= pui8Buf[i];
  }

  return value;
}

/*---------------------------------------------------------------------------*/

static bd_bool_t bd_getInfo( bd_uint16_t tag, BD_Type* pType, char* pName, bd_size_t bufLen )
{
  bd_bool_t rc    = BD_FALSE;    /* assume error */
  int       index;

  for ( index = 0; bd_info[index].tag != BD_End; index++ )
  {
    if ( bd_info[index].tag == tag )
    {
      BD_ASSERT( bd_info[index].pName != 0 );

      /* Found desired entry */
      rc = BD_TRUE;

      if( pType != 0 )
      {
        /* Fill in type information */
        *pType = bd_info[index].type;
      }

      if( pName != 0 )
      {
        /* Fill in tag name */
        bd_safeStrCpy( pName, bufLen, bd_info[index].pName, strlen(bd_info[index].pName) );
      }

      break;
    }
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

static bd_bool_t bd_getNextEntry( const BD_Context* pCtx, BD_Entry* pEntry )
{
  bd_bool_t         rc      = BD_FALSE;    /* assume error */
  const bd_uint8_t* pTemp   = 0;
  bd_uint16_t       currTag = 0;
  bd_uint16_t       currLen = 0;
  bd_bool_t         first   = BD_FALSE;    /* assume error */

  BD_ASSERT( pCtx != 0 );
  BD_ASSERT( pEntry != 0 );

  /* Make sure we don't read beyond entry is NULL */
  if ( pEntry == 0 )
  {
    return rc;
  }

  /* Check for last entry*/
  if ( pEntry->entry >= pCtx->entries )
  {
    return rc;
  }

  /* For the first entry starts with the context buffer else the entry buffer minus header 4 bytes */
  if ( ( pEntry->pData == 0 ) )
  {
    pTemp = pCtx->pData;
    first = BD_TRUE;
  }
  else
  {
    pTemp = pEntry->pData - 4;
    first = BD_FALSE;
  }

  /* Make sure we don't read beyond data buffer is 0 */
  if ( pTemp == 0 )
  {
    BD_ASSERT(BD_FALSE);
    return rc;
  }

  /* Make sure we don't read beyond data buffer is valid range */
  if ( pTemp < pCtx->pData )
  {
    BD_ASSERT(BD_FALSE);
    return rc;
  }

  /* Make sure we don't read beyond data buffer when getting next tag/len */
  if ( (pTemp + 4) > pCtx->pDataEnd )
  {
    BD_ASSERT(BD_FALSE);
    return rc;
  }

  /* Read tag/length of current entry */
  currTag = bd_getUInt16( pTemp + 0 );
  currLen = bd_getUInt16( pTemp + 2 );

  /* Validate length field */
  if ( currLen > BD_MAX_ENTRY_LEN )
  {
    BD_ASSERT(BD_FALSE);
    return rc;
  }

  /* Must not exceed data buffer */
  if ( (pTemp + 4 + currLen) > pCtx->pDataEnd )
  {
    BD_ASSERT(BD_FALSE);
    return rc;
  }

  if( first==BD_FALSE )
  {
    /* Advance to next entry */
    pTemp += (4 + currLen);

    /* Make sure we don't read beyond data buffer when getting next tag/len */
    if ( (pTemp + 4) > pCtx->pDataEnd )
    {
      BD_ASSERT(BD_FALSE);
      return rc;
    }

    /* Read tag/length of current entry */
    currTag = bd_getUInt16( pTemp + 0 );
    currLen = bd_getUInt16( pTemp + 2 );

    /* Validate length field */
    if ( currLen > BD_MAX_ENTRY_LEN )
    {
      BD_ASSERT(BD_FALSE);
      return rc;
    }
  }

  /* Fill the entry structure */
  pEntry->tag   = currTag;
  pEntry->len   = currLen;
  pEntry->entry++;
  pEntry->pData = pTemp + 4;

  rc = BD_TRUE;
  return rc;
}

/*---------------------------------------------------------------------------*/

static const void* bd_findEntry( const BD_Context* pCtx, bd_uint16_t tag,
                                 bd_uint_t index, bd_uint16_t* pLen )
{
  const void*         pResult   = 0;
  const bd_uint8_t*   pTemp     = 0;
  bd_uint_t           currIndex = 0;
  bd_uint_t           currEntry = 0;


  BD_ASSERT( pCtx != 0 );
  BD_ASSERT( pCtx->initialized );
  BD_ASSERT( index < pCtx->entries );
  /* pLen is allowed to be 0, therefore no check is required. */

  if ( pLen != 0 )
  {
    *pLen = 0;
  }

  /* Try to find desired entry in data buffer */

  pTemp = (const bd_uint8_t*)pCtx->pData;

  for (currEntry = 0; currEntry < pCtx->entries; currEntry++)
  {
    bd_uint16_t  currTag = 0;
    bd_uint16_t  currLen = 0;

    /* Make sure we don't read beyond data buffer when getting next tag/len */
    if ( (pTemp + 4) > pCtx->pDataEnd )
    {
      BD_ASSERT(BD_FALSE);
      break;
    }

    /* Read tag/length of current entry */
    currTag = bd_getUInt16( pTemp + 0 );
    currLen = bd_getUInt16( pTemp + 2 );

    /* Validate length field */
    if ( currLen > BD_MAX_ENTRY_LEN )
    {
      BD_ASSERT(BD_FALSE);
      break;
    }

    /* Must not exceed data buffer */
    if ( (pTemp + 4 + currLen) > pCtx->pDataEnd )
    {
      BD_ASSERT(BD_FALSE);
      break;
    }

    if ( currTag == tag )
    {
      /* Desired entry found */
      if ( currIndex == index )
      {
        /* Desired entry found (tag and index match) */
        /* Return pointer to data area of entry */
        pResult = pTemp + 4;
        if ( pLen != 0 )
        {
          *pLen = currLen;
        }
        break;
      }
      else
      {
        /* Not yet the entry we wanted, look further */
        currIndex++;
      }
    }
    else if ( currTag == 0x0000 )
    {
      /* Sorry, entry not found -> aborting */
      break;
    }

    /* Advance to next entry */
    pTemp += (4 + currLen);
  }

  return pResult;
}


/*--- global functions ------------------------------------------------------*/

bd_bool_t BD_CheckHeader( BD_Context* pCtx, const void* pHeader )
{
  const char*      pTemp = 0;
  bd_bool_t        rc    = BD_FALSE;    /* assume error */

  /* Argument check */

  if (    (pCtx    == 0)
       || (pHeader == 0)
     )
  {
    return BD_FALSE;
  }

  /* Initialize context */

  pCtx->headerOk    = BD_FALSE;
  pCtx->initialized = BD_FALSE;
  pCtx->size        = 0;
  pCtx->entries     = 0;
  pCtx->pData       = 0;
  pCtx->pDataEnd    = 0;

  /* Check Identification */

  pTemp = (const char*)pHeader;
  if ( (pTemp[0] == 'B') && (pTemp[1] == 'D') && (pTemp[2] == 'V') && (pTemp[3] == '1') )
  {
    bd_uint16_t  payloadLen = 0;
    bd_uint16_t  checksum = 0;

    /* Valid ID -> Read payload length */

    payloadLen = bd_getUInt16( pTemp+4 );
    checksum = bd_getUInt16( pTemp+6 );

    /* Validate length, abort on unreasonable values */

    if ( payloadLen <= BD_MAX_LENGTH )
    {
      pCtx->size        = payloadLen;
      pCtx->checksum    = checksum;
      pCtx->headerOk    = BD_TRUE;

      rc = BD_TRUE;
    }
    else
    {
      /* Invalid length */
    }
  }
  else
  {
    /* Unknown identification */
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_ImportData( BD_Context* pCtx, const void* pData )
{
  const bd_uint8_t*   pTemp = 0;
  bd_bool_t           rc    = BD_FALSE;    /* assume error */
  bd_uint16_t         tmpChecksum = 0;
  size_t              i;

  /* Argument check */

  if (    (pCtx  == 0)
       || (pData == 0)
       || !pCtx->headerOk         /* BD_CheckHeader() has not yet been called */
     )
  {
    return BD_FALSE;
  }

  /* Scan through data, check consistency */

  BD_ASSERT( pCtx->size <= BD_MAX_LENGTH );

  pCtx->entries   = 0;
  pCtx->pData     = (const bd_uint8_t*)pData;
  pCtx->pDataEnd  = pCtx->pData + pCtx->size;

  pTemp = (const bd_uint8_t*)pCtx->pData;
  for (;;)
  {
    bd_uint16_t  tag = 0;
    bd_uint16_t  len = 0;

    /* Make sure we don't read beyond data buffer when getting next tag/len */
    if ( (pTemp + 4) > pCtx->pDataEnd )
    {
      /* @@@ RS: This would be an error */
      break;
    }

    /* Read tag/length of current entry */
    tag = bd_getUInt16( pTemp + 0 );
    len = bd_getUInt16( pTemp + 2 );
    pTemp += 4;

    /* Validate length field */
    if ( len > BD_MAX_ENTRY_LEN )
    {
      /* @@@ RS: This would be an error */
      break;
    }

    /* Must not exceed data buffer */
    if ( (pTemp + len) > pCtx->pDataEnd )
    {
      /* @@@ RS: This would be an error */
      break;
    }

    /* Stop if end tag found */
    if ( tag == 0x0000 )
    {
      rc = BD_TRUE;
      break;
    }

    /* Advance to next entry */
    pTemp += len;

    pCtx->entries++;
  }

  /* If parsing was Ok and header contained a checksum, verify it */
  if ( rc && ( pCtx->checksum != 0 ) )
  {
    /* Reset pointer to the start of the data/payload buffer */
    pTemp = (const bd_uint8_t*)pCtx->pData;

    /* Compute running byte checksum */
    for (i = 0; i < pCtx->size; i++)
    {
      tmpChecksum = (bd_uint16_t)( (tmpChecksum + pTemp[i]) & 0xFFFF );
    }

    if ( tmpChecksum != pCtx->checksum )
    {
      /* Checksum does not match */
      rc = BD_FALSE;
    }
  }

  if ( rc )
  {
    /* Everything ok */
    pCtx->initialized = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_ExistsEntry( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index )
{
  const void*   pData   = 0;           /* pointer to entry data */
  bd_bool_t     rc      = BD_FALSE;    /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, 0 );
  if ( pData != 0 )
  {
    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetInfo( bd_uint16_t tag, BD_Type* pType, char *pName, bd_size_t bufLen )
{
  /*
   * It is allowed to call this function with either pType or pName
   * set to 0. In this case the fields will not be filled out
   */
  return bd_getInfo( tag, pType, pName, bufLen );
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_InitEntry( BD_Entry* pEntry )
{
  /* Argument check */
  if ( pEntry == 0 )
  {
    return BD_FALSE;
  }

  memset(pEntry, 0, sizeof(BD_Entry));

  return BD_TRUE;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetNextEntry( const BD_Context* pCtx, BD_Entry* pEntry )
{
  /* Argument check */

  if (    (pCtx == 0)
       || !pCtx->initialized
       || (pEntry == 0)
     )
  {
    return BD_FALSE;
  }

  return bd_getNextEntry( pCtx, pEntry );
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetVoid( const BD_Context* pCtx, bd_uint16_t tag,
                      bd_uint_t index, bd_bool_t* pResult )
{
  const void*   pData   = 0;            /* pointer to entry data */
  bd_uint16_t   len     = 0;            /* len of entry (should be 2) */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = BD_FALSE;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );

  /* Void tags necessarily have a length of 0 */
  if ( (pData != 0) && (len == 0) )
  {
    *pResult = BD_TRUE;
  }

  return BD_TRUE;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetUInt8( const BD_Context* pCtx, bd_uint16_t tag,
                       bd_uint_t index, bd_uint8_t* pResult )
{
  const void*   pData   = 0;            /* pointer to entry data */
  bd_uint16_t   len     = 0;            /* len of entry (should be 2) */
  bd_bool_t     rc      = BD_FALSE;     /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 1) )
  {
    *pResult = *(bd_uint8_t*)pData;
    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetUInt16( const BD_Context* pCtx, bd_uint16_t tag,
                        bd_uint_t index, bd_uint16_t* pResult )
{
  const void*   pData   = 0;          /* pointer to entry data */
  bd_uint16_t   len     = 0;          /* len of entry (should be 2) */
  bd_bool_t     rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 2) )
  {
    *pResult = bd_getUInt16( pData );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetUInt32( const BD_Context* pCtx, bd_uint16_t tag,
                        bd_uint_t index, bd_uint32_t* pResult )
{
  const void*     pData   = 0;          /* pointer to entry data */
  bd_uint16_t     len     = 0;          /* len of entry (should be 4) */
  bd_bool_t       rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 4) )
  {
    *pResult = bd_getUInt32( pData );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetUInt64( const BD_Context* pCtx, bd_uint16_t tag,
                        bd_uint_t index, bd_uint64_t* pResult )
{
  const void*     pData   = 0;          /* pointer to entry data */
  bd_uint16_t     len     = 0;          /* len of entry (should be 4) */
  bd_bool_t       rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 8) )
  {
    *pResult = bd_getUInt64( pData );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetIPv4( const BD_Context* pCtx, bd_uint16_t tag,
                      bd_uint_t index, bd_uint32_t* pResult )
{
  const void*   pData   = 0;          /* pointer to entry data */
  bd_uint16_t   len     = 0;          /* len of entry (should be 4) */
  bd_bool_t     rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  *pResult = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 4) )
  {
    *pResult = bd_getUInt32( pData );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetMAC( const BD_Context* pCtx, bd_uint16_t tag,
                     bd_uint_t index, bd_uint8_t pResult[6] )
{
  const void*   pData   = 0;          /* pointer to entry data */
  bd_uint16_t   len     = 0;          /* len of entry (should be 4) */
  bd_bool_t     rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  memset( pResult, 0x00, 6 );

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 6) )
  {
    memcpy( pResult, pData, 6 );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetString( const BD_Context* pCtx, bd_uint16_t tag,
                        bd_uint_t index, char* pResult, bd_size_t bufLen )
{
  const void*   pData   = 0;          /* pointer to entry data */
  bd_uint16_t   len     = 0;
  bd_bool_t     rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
       || (bufLen < 1)
     )
  {
    return BD_FALSE;
  }

  BD_ASSERT( bufLen <= BD_MAX_ENTRY_LEN );

  /* Clear result */
  pResult[0] = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( pData != 0 )
  {
    bd_safeStrCpy( pResult, bufLen, pData, len );

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetBlob( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index,
                      char* pResult, bd_size_t bufLen, bd_size_t* pReadLen)
{
  const void*   pData   = 0;          /* pointer to entry data */
  bd_uint16_t   len     = 0;          /* len of entry */
  bd_bool_t     rc      = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || (pReadLen == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
       || (bufLen < 1)
     )
  {
    return BD_FALSE;
  }

  BD_ASSERT( bufLen <= BD_MAX_ENTRY_LEN );

  /* Initialize output value */
  *pReadLen = 0;

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len <= bufLen) )
  {
    /* Copy binary data to user provided buffer */
    memcpy( pResult, pData, len );
    *pReadLen = len;

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetPartition( const BD_Context* pCtx, bd_uint16_t tag,
                           bd_uint_t index, BD_PartitionEntry* pResult )
{
  const bd_uint8_t* pData       = 0;          /* pointer to entry data */
  bd_uint16_t       len         = 0;          /* len of entry */
  bd_bool_t         rc          = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  /* Clear result */
  memset( pResult, 0x00, sizeof(BD_PartitionEntry) );

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );

  /* Size must be at least 10 to make sure we have all mandatory fields */
  if ( ( pData != 0 ) && ( len >= 10 ) )
  {
    /* Copy the fix sized fields */
    pResult->flags  = *pData;
    pData++;
    pResult->type   = *pData;
    pData++;
    pResult->offset = bd_getUInt32(pData);
    pData+=sizeof(bd_uint32_t);
    pResult->size   = bd_getUInt32(pData);
    pData+=sizeof(bd_uint32_t);

    /* Copy the partition name */
    len -= 10;
    bd_safeStrCpy(pResult->name, sizeof(pResult->name), (const char*)pData, len);

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

bd_bool_t BD_GetPartition64( const BD_Context* pCtx, bd_uint16_t tag,
                             bd_uint_t index, BD_PartitionEntry64* pResult )
{
  const bd_uint8_t* pData       = 0;          /* pointer to entry data */
  bd_uint16_t       len         = 0;          /* len of entry */
  bd_bool_t         rc          = BD_FALSE;   /* assume error */

  /* Argument check */

    if (pCtx == 0) {
        return BD_FALSE;
    }
    writel(1<<14, 0x4804c13c); /* set gpio out */
  if ( (pResult == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
mdelay(1000);
writel(1<<14, 0x4804c190); /* set gpio out */
    return BD_FALSE;
  }

mdelay(1000);
writel(1<<14, 0x4804c190); /* set gpio out */
  /* Clear result */
  memset( pResult, 0x00, sizeof(BD_PartitionEntry64) );

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );

  /* Size must be at least 16 to make sure we have all mandatory fields */
  if ( ( pData != 0 ) && (len >= 16) )
  {
    /* Copy the fix sized fields */
    pResult->flags    = *pData;
    pData++;
    pResult->type     = *pData;
    pData++;
    pResult->options  = *pData;
    pData++;
    pData+=5;
    pResult->offset   = bd_getUInt64(pData);
    pData+=sizeof(bd_uint64_t);
    pResult->size     = bd_getUInt64(pData);
    pData+=sizeof(bd_uint64_t);

    /* Copy the partition name */
    len -= 16;
    bd_safeStrCpy(pResult->name, sizeof(pResult->name), (const char*)pData, len);

    rc = BD_TRUE;
  }

  return rc;
}

/*---------------------------------------------------------------------------*/

#ifdef BD_CONF_HAS_HASH

bd_bool_t BD_VerifySha1Hmac( const BD_Context* pCtx, bd_uint16_t tag, bd_uint_t index, const void* pKey, bd_size_t keyLen )
{
  const bd_uint8_t* pData       = 0;          /* pointer to entry data */
  bd_uint16_t       len         = 0;          /* len of entry */
  bd_uint16_t       hashDataLen = 0;          /* number of bytes to hash */
  const bd_uint8_t* pRefHash;
  bd_uint8_t        computedHash[160/8];
  bd_bool_t         rc          = BD_FALSE;   /* assume error */

  /* Argument check */

  if (    (pCtx == 0)
       || (pKey == 0)
       || !pCtx->initialized
       || (index >= pCtx->entries)
     )
  {
    return BD_FALSE;
  }

  BD_ASSERT((keyLen >= 4) && (keyLen <= 256));

  /* Try to find desired entry */
  pData = bd_findEntry( pCtx, tag, index, &len );
  if ( (pData != 0) && (len == 6) )
  {
    /* Determine amount of data to hash */
    hashDataLen = bd_getUInt16(pData);
    pData += 2;
    BD_ASSERT( hashDataLen < BD_MAX_LENGTH );

    /* Remember hash value specified in tag */
    pRefHash = pData;
    pData += 4;

    /* Compute hash over specified range */
    BD_SHA1_HASH_FUNC(pKey, keyLen, pData, hashDataLen, computedHash);

    /* Compare computed hash value with the one stored in the tag */
    if (memcmp(computedHash, pRefHash, 4) == 0)
    {
      /* Ok -> Hashes match */
      rc = BD_TRUE;
    }
  }

  return rc;
}

#endif /* BD_CONF_HAS_HASH */


/*--- unit test -------------------------------------------------------------*/

#ifdef BD_CONF_UNIT_TESTS

static void bd_testHelpers( void )
{
  static const bd_uint8_t data[] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 };
  bd_uint64_t             t64;
  bd_uint32_t             t32;
  bd_uint16_t             t16;
  char                    tempBuf[4];

  t16 = bd_getUInt16( data );
  BD_ASSERT( t16 == 0x1234 );

  t32 = bd_getUInt32( data );
  BD_ASSERT( t32 == 0x12345678 );

  t64 = bd_getUInt64( data );
  BD_ASSERT( t64 == 0x123456789abcdef0ULL );

  memset(tempBuf, 0xAA, 4);
  bd_safeStrCpy(tempBuf, 4, "xy", 2);
  BD_ASSERT(tempBuf[0] == 'x');
  BD_ASSERT(tempBuf[1] == 'y');
  BD_ASSERT(tempBuf[2] == 0);

  memset(tempBuf, 0xAA, 4);
  bd_safeStrCpy(tempBuf, 4, "abc", 3);
  BD_ASSERT(tempBuf[2] == 'c');
  BD_ASSERT(tempBuf[3] == 0);

  memset(tempBuf, 0xAA, 4);
  bd_safeStrCpy(tempBuf, 4, "bcde", 4);
  BD_ASSERT(tempBuf[2] == 'd');
  BD_ASSERT(tempBuf[3] == 0);

  memset(tempBuf, 0xAA, 4);
  bd_safeStrCpy(tempBuf, 4, "defghi", 6);
  BD_ASSERT(tempBuf[2] == 'f');
  BD_ASSERT(tempBuf[3] == 0);
}

/*---------------------------------------------------------------------------*/

static void bd_testCheckHeader( void )
{
  static const bd_uint8_t hdrGood[] = { 'B', 'D', 'V', '1', 0x00, 0x10, 0x00, 0x00 };
  static const bd_uint8_t hdrBad1[] = { 'B', 'X', 'Y', 'Z', 0x00, 0x10, 0x00, 0x00 };
  static const bd_uint8_t hdrBad2[] = { 'B', 'D', 'V', '1', 0x10, 0x01, 0x00, 0x00 };

  BD_Context    bdCtx;
  bd_bool_t     rc;


  /* No context, no header */
  rc = BD_CheckHeader( 0, 0 );
  BD_ASSERT( !rc );

  /* No header */
  rc = BD_CheckHeader( &bdCtx, 0 );
  BD_ASSERT( !rc );

  /* No context */
  rc = BD_CheckHeader( 0, hdrGood );
  BD_ASSERT( !rc );

  /* Invalid header (identification) */
  rc = BD_CheckHeader( &bdCtx, hdrBad1 );
  BD_ASSERT( !rc );

  /* Invalid header (length) */
  rc = BD_CheckHeader( &bdCtx, hdrBad2 );
  BD_ASSERT( !rc );

  /* Ok */
  rc = BD_CheckHeader( &bdCtx, hdrGood );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.headerOk );
  BD_ASSERT( bdCtx.size == 16 );
}

/*---------------------------------------------------------------------------*/

static void bd_testImport( void )
{
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, 0x04, 0x00, 0x00 };
  static const bd_uint8_t data1[]  = { 0x00, 0x00, 0x00, 0x00 };

  static const bd_uint8_t hdr2[]   = { 'B', 'D', 'V', '1', 0x00, 13, 0x00, 0x00 };
  static const bd_uint8_t data2[]  = { 0x00, 0x01, 0x00, 0x05, 'A', 'B', 'C', 'D', 'E', 0x00, 0x00, 0x00, 0x00 };

  BD_Context    bdCtx;
  bd_bool_t     rc;

  /* Case 0: End tag only */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 0 );

  /* Case 1: One string entry, end tag */
  rc = BD_CheckHeader( &bdCtx, hdr2 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data2 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );
}

/*---------------------------------------------------------------------------*/

static void bd_testChecksum( void )
{
  /* 1 void tag + end tag with valid checksum */
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, 0x08, 0x00, 0x04 };
  static const bd_uint8_t data1[]  = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  /* 1 void tag + end tag with invalid checksum */
  static const bd_uint8_t hdr2[]   = { 'B', 'D', 'V', '1', 0x00, 0x08, 0x00, 0x02 };
  static const bd_uint8_t data2[]  = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  /* 1 void tag + end tag without checksum */
  static const bd_uint8_t hdr3[]   = { 'B', 'D', 'V', '1', 0x00, 0x08, 0x00, 0x00 };
  static const bd_uint8_t data3[]  = { 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  BD_Context    bdCtx;
  bd_bool_t     rc;


  /* Case 0: valid checksum */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );
  BD_ASSERT( bdCtx.checksum == 0x4 );

  /* Case 1: invalid checksum */
  rc = BD_CheckHeader( &bdCtx, hdr2 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data2 );
  BD_ASSERT( !rc );

  /* Case 2: no checksum */
  rc = BD_CheckHeader( &bdCtx, hdr3 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data3 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );
  BD_ASSERT( bdCtx.checksum == 0 );
}

/*---------------------------------------------------------------------------*/

static void bd_testGetUInt( void )
{
  static const bd_uint8_t data1[]  = { 0x00, 0x01, 0x00, 0x04, 0x12, 0x34, 0x56, 0x78,
                                    0x00, 0x01, 0x00, 0x04, 0xca, 0xfe, 0xba, 0xbe,
                                    0x00, 0x02, 0x00, 0x02, 0x47, 0x11,
                                    0x00, 0x00, 0x00, 0x00 };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data1), 0x00, 0x00 };

  BD_Context    bdCtx;
  bd_uint32_t   result32;
  bd_uint16_t   result16;
  bd_bool_t     rc;


  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.entries == 3 );


  /* Unknown tag */
  rc = BD_ExistsEntry( &bdCtx, 1001, 0 );
  BD_ASSERT( !rc );

  /* Unknown index */
  rc = BD_ExistsEntry( &bdCtx, 1, 2 );
  BD_ASSERT( !rc );

  /* Ok */
  rc = BD_ExistsEntry( &bdCtx, 1, 0 );
  BD_ASSERT( rc );

  rc = BD_ExistsEntry( &bdCtx, 1, 1 );
  BD_ASSERT( rc );

  rc = BD_ExistsEntry( &bdCtx, 2, 0 );
  BD_ASSERT( rc );


  /* No context */
  rc = BD_GetUInt32( 0, 1, 0, &result32 );
  BD_ASSERT( !rc );

  /* No result pointer */
  rc = BD_GetUInt32( &bdCtx, 1, 0, 0 );
  BD_ASSERT( !rc );

  /* Index > number of entries */
  rc = BD_GetUInt32( &bdCtx, 1, 3, &result32 );
  BD_ASSERT( !rc );

  /* Unknown tag */
  rc = BD_GetUInt32( &bdCtx, 1001, 0, &result32 );
  BD_ASSERT( !rc );

  /* Entry with wrong size */
  rc = BD_GetUInt32( &bdCtx, 2, 0, &result32 );
  BD_ASSERT( !rc );

  /* Ok: First entry */
  rc = BD_GetUInt32( &bdCtx, 1, 0, &result32 );
  BD_ASSERT( rc );
  BD_ASSERT( result32 == 0x12345678 );

  /* Ok: Second entry */
  rc = BD_GetUInt32( &bdCtx, 1, 1, &result32 );
  BD_ASSERT( rc );
  BD_ASSERT( result32 == 0xcafebabe );

  /* Ok: */
  rc = BD_GetUInt16( &bdCtx, 2, 0, &result16 );
  BD_ASSERT( rc );
  BD_ASSERT( result16 == 0x4711 );
}

/*---------------------------------------------------------------------------*/

static void bd_testGetString( void )
{
  static const bd_uint8_t data1[]  = { 0x00, 0x01, 0x00, 0x05, 'A', 'B', 'C', 'D', 'E', 0x00, 0x00, 0x00, 0x00 };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data1), 0x00, 0x00 };

  BD_Context  bdCtx;
  char        buffer[16];
  bd_bool_t   rc;


  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );

  /* Zero sized buffer */
  rc = BD_GetString( &bdCtx, 1, 0, buffer, 0 );
  BD_ASSERT(!rc);

  /* One byte buffer (terminating 0 only) */
  memset(buffer, 0xCC, sizeof(buffer));
  rc = BD_GetString( &bdCtx, 1, 0, buffer, 1 );
  BD_ASSERT(rc);
  BD_ASSERT(buffer[0] == 0);

  /* String exactly fits buffer */
  memset(buffer, 0xCC, sizeof(buffer));
  rc = BD_GetString( &bdCtx, 1, 0, buffer, 6 );
  BD_ASSERT(rc);
  BD_ASSERT(buffer[4] == 'E');
  BD_ASSERT(buffer[5] == 0);

  /* Buffer one too short */
  memset(buffer, 0xCC, sizeof(buffer));
  rc = BD_GetString( &bdCtx, 1, 0, buffer, 5 );
  BD_ASSERT(rc);
  BD_ASSERT(buffer[3] == 'D');
  BD_ASSERT(buffer[4] == 0);
}

/*---------------------------------------------------------------------------*/

static void bd_testGetMAC( void )
{
  static const bd_uint8_t data1[]  = { 0x00, 0x11, 0x00, 0x06, 0x00, 0xA0, 0xBA, 0x12, 0x34, 0x56,
                                    0x00, 0x00, 0x00, 0x00 };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data1), 0x00, 0x00 };

  BD_Context  bdCtx;
  bd_uint8_t  buffer[6];
  bd_bool_t   rc;


  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );

  /* Non-existing entry */
  memset(buffer, 0xCC, sizeof(buffer));
  rc = BD_GetMAC( &bdCtx, 1017, 0, buffer );
  BD_ASSERT(!rc);
  BD_ASSERT(buffer[0] == 0);
  BD_ASSERT(buffer[1] == 0);
  BD_ASSERT(buffer[2] == 0);
  BD_ASSERT(buffer[3] == 0);
  BD_ASSERT(buffer[4] == 0);
  BD_ASSERT(buffer[5] == 0);

  /* Ok */
  memset(buffer, 0xCC, sizeof(buffer));
  rc = BD_GetMAC( &bdCtx, 17, 0, buffer );
  BD_ASSERT(rc);
  BD_ASSERT(buffer[0] == 0x00);
  BD_ASSERT(buffer[1] == 0xA0);
  BD_ASSERT(buffer[2] == 0xBA);
  BD_ASSERT(buffer[3] == 0x12);
  BD_ASSERT(buffer[4] == 0x34);
  BD_ASSERT(buffer[5] == 0x56);
}

/*---------------------------------------------------------------------------*/

static void bd_testGetIPv4( void )
{                                 /* IP = 192.168.2.1 = 0xC0A80201 */
  static const bd_uint8_t data1[]  = { 0x00, 0x12, 0x00, 0x04, 0xC0, 0xA8, 0x02, 0x01,
                                    0x00, 0x00, 0x00, 0x00 };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data1), 0x00, 0x00 };

  BD_Context    bdCtx;
  bd_uint32_t   ipAddr;
  bd_bool_t     rc;


  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );

  /* Ok */
  ipAddr = 0;
  rc = BD_GetIPv4( &bdCtx, 18, 0, &ipAddr );
  BD_ASSERT(rc);
  BD_ASSERT(ipAddr == 0xC0A80201);
}

/*---------------------------------------------------------------------------*/

static void bd_testGetPartition( void )
{
  /* Name with length zero */
  static const bd_uint8_t data1[]  = { 0x00, 0x18, 0x00, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                       0x09, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00 };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data1), 0x00, 0x00 };

  /* Name with length 5 */
  static const bd_uint8_t data2[]  = { 0x00, 0x18, 0x00, 0x0F, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                       0x09, 0x0a, 'N',  'a',  'm',  'e',  '5',  0x00, 0x00, 0x00, 0x00};
  static const bd_uint8_t hdr2[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data2), 0x00, 0x00 };

  /* Name with length 16 */
  static const bd_uint8_t data3[]  = { 0x00, 0x18, 0x00, 0x1A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                       0x09, 0x0a, '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',
                                       'a',  'b',  'c',  'd',  'e',  'f',  0x00,  0x00, 0x00, 0x00  };
  static const bd_uint8_t hdr3[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data3), 0x00, 0x00 };

  /* Name with length 17 */
  static const bd_uint8_t data4[]  = { 0x00, 0x18, 0x00, 0x1B, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                       0x09, 0x0a, '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',
                                       'A',  'B',  'C',  'D',  'E',  'F',  'x',  0x00,  0x00, 0x00, 0x00  };
  static const bd_uint8_t hdr4[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data4), 0x00, 0x00 };

  /* Too short BD */
  static const bd_uint8_t data5[]  = { 0x00, 0x18, 0x00, 0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                       0x09, 0x00, 0x00 };
  static const bd_uint8_t hdr5[]   = { 'B', 'D', 'V', '1', 0x00, sizeof(data5), 0x00, 0x00, 0x00, 0x00 };

  BD_Context  bdCtx;
  BD_PartitionEntry  bd_partitionEntry;
  bd_bool_t   rc;

  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == 1 );

  /* 0 buffer */
  rc = BD_GetPartition( &bdCtx, BD_Partition, 0, 0 );
  BD_ASSERT(!rc);

  /* Check values */
  rc = BD_GetPartition( &bdCtx, BD_Partition, 0, &bd_partitionEntry );
  BD_ASSERT(rc);
  BD_ASSERT(bd_partitionEntry.flags  == 0x01);
  BD_ASSERT(bd_partitionEntry.type   == 0x02);
  BD_ASSERT(bd_partitionEntry.size   == 0x0708090a);
  BD_ASSERT(bd_partitionEntry.offset == 0x03040506);

  /* Check string length is 0 */
  BD_ASSERT(bd_partitionEntry.name[0] == 0);

  /* Check string length in bd 5 */
  rc = BD_CheckHeader( &bdCtx, hdr2 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data2 );
  BD_ASSERT( rc );
  rc = BD_GetPartition( &bdCtx, BD_Partition, 0, &bd_partitionEntry );
  BD_ASSERT(rc);
  BD_ASSERT(bd_partitionEntry.name[0] == 'N');
  BD_ASSERT(bd_partitionEntry.name[4] == '5');
  BD_ASSERT(bd_partitionEntry.name[5] == 0);

  /* Check string length in bd 16*/
  rc = BD_CheckHeader( &bdCtx, hdr3 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data3 );
  BD_ASSERT( rc );
  rc = BD_GetPartition( &bdCtx, BD_Partition, 0, &bd_partitionEntry );
  BD_ASSERT(rc);
  BD_ASSERT(bd_partitionEntry.name[0] == '0');
  BD_ASSERT(bd_partitionEntry.name[15] == 'f');
  BD_ASSERT(bd_partitionEntry.name[16] == 0);

  /* Check string length in bd 17 */
  rc = BD_CheckHeader( &bdCtx, hdr4 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data4 );
  BD_ASSERT( rc );
  rc = BD_GetPartition( &bdCtx, BD_Partition, 0, &bd_partitionEntry );
  BD_ASSERT(rc);
  BD_ASSERT(bd_partitionEntry.name[0] == '0');
  BD_ASSERT(bd_partitionEntry.name[15] == 'F');
  BD_ASSERT(bd_partitionEntry.name[16] == 0);

  /* BD too short */
  rc = BD_CheckHeader( &bdCtx, hdr5 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data5 );
  BD_ASSERT( !rc );
}

/*---------------------------------------------------------------------------*/

static void bd_testGetEntry( void )
{
  #define BD16(var)   (bd_uint8_t)(((var) >> 8) & 0x00FF), (bd_uint8_t)(((var) >> 0) & 0x00FF)

  typedef struct _BD_IndexTable
  {
    bd_uint16_t tag;                 /**< Tag of entry */
    bd_uint_t   index;               /**< Index of entry */
  }
  BD_IndexTable;

  static const bd_uint_t num_entries = 51; // without end tag
  static const bd_uint8_t data1[]  =
  {
    // serial[0]: 'Serial'
    (bd_uint8_t)(BD_Serial>>8), (bd_uint8_t)(BD_Serial>>0), 0x00, 0x06,
    'S', 'e', 'r', 'i', 'a', 'l',
    // date[0]: '01.01.2000'
    (bd_uint8_t)(BD_Production_Date>>8), (bd_uint8_t)(BD_Production_Date>>0), 0x00, 0x0A,
    '0', '1', '.', '0', '1', '.', '2', '0', '0', '0',
    // version[0]: 1
    (bd_uint8_t)(BD_Hw_Ver>>8), (bd_uint8_t)(BD_Hw_Ver>>0), 0x00, sizeof(bd_uint8_t),
    0x01,
    // release[0]: 0
    (bd_uint8_t)(BD_Hw_Rel>>8), (bd_uint8_t)(BD_Hw_Rel>>0), 0x00, sizeof(bd_uint8_t),
    0x00,
    // name[0]: 'Product'
    (bd_uint8_t)(BD_Prod_Name>>8), (bd_uint8_t)(BD_Prod_Name>>0), 0x00, 0x07,
    'P', 'r', 'o', 'd', 'u', 'c', 't',
    // variant[0]: 0xF0A5
    (bd_uint8_t)(BD_Prod_Variant>>8), (bd_uint8_t)(BD_Prod_Variant>>0), 0x00, sizeof(bd_uint16_t),
    0xF0, 0xA5,
    // compatibility[0]: 'Comp'
    (bd_uint8_t)(BD_Prod_Compatibility>>8), (bd_uint8_t)(BD_Prod_Compatibility>>0), 0x00, 0x04,
    'C', 'o', 'm', 'p',
    // mac 05:14:23:32:41:50
    (bd_uint8_t)(BD_Eth_Mac>>8), (bd_uint8_t)(BD_Eth_Mac>>0), 0x00, 0x06,
    0x05, 0x14, 0x23, 0x32, 0x41, 0x50,
    // ipv4-addr[0]: 192.168.0.2
    (bd_uint8_t)(BD_Ip_Addr>>8), (bd_uint8_t)(BD_Ip_Addr>>0), 0x00, sizeof(bd_uint32_t),
    192, 168, 0, 2,
    // ipv4-mask[0]: 255.255.255.0
    (bd_uint8_t)(BD_Ip_Netmask>>8), (bd_uint8_t)(BD_Ip_Netmask>>0), 0x00, sizeof(bd_uint32_t),
    255, 255, 255, 0,

    // ipv4-gateway[0]: 192.168.0.1
    (bd_uint8_t)(BD_Ip_Gateway>>8), (bd_uint8_t)(BD_Ip_Gateway>>0), 0x00, sizeof(bd_uint32_t),
    192, 168, 0, 1,
    // ipv4-addr[0]: 172.20.0.2
    (bd_uint8_t)(BD_Ip_Addr>>8), (bd_uint8_t)(BD_Ip_Addr>>0), 0x00, sizeof(bd_uint32_t),
    172, 20, 0, 2,
    // ipv4-mask[0]: 255.255.0.0
    (bd_uint8_t)(BD_Ip_Netmask>>8), (bd_uint8_t)(BD_Ip_Netmask>>0), 0x00, sizeof(bd_uint32_t),
    255, 255, 0, 0,
    // ipv4-gateway[0]: 172.20.0.1
    (bd_uint8_t)(BD_Ip_Gateway>>8), (bd_uint8_t)(BD_Ip_Gateway>>0), 0x00, sizeof(bd_uint32_t),
    172, 20, 0, 1,
    // usbd-pid[0]: 0xAABB
    (bd_uint8_t)(BD_Usb_Device_Id>>8), (bd_uint8_t)(BD_Usb_Device_Id>>0), 0x00, sizeof(bd_uint16_t),
    0xAA, 0xBB,
    // usbd-vid[0]: 0xCCDD
    (bd_uint8_t)(BD_Usb_Vendor_Id>>8), (bd_uint8_t)(BD_Usb_Vendor_Id>>0), 0x00, sizeof(bd_uint16_t),
    0xCC, 0xDD,
    // ram-size[0]: 0xA0A1A2A3
    (bd_uint8_t)(BD_Ram_Size>>8), (bd_uint8_t)(BD_Ram_Size>>0), 0x00, sizeof(bd_uint32_t),
    0xA0, 0xA1, 0xA2, 0xA3,
    // ram-size64[0]: 0xB0B1B2B3B4B5B6B7
    (bd_uint8_t)(BD_Ram_Size64>>8), (bd_uint8_t)(BD_Ram_Size64>>0), 0x00, sizeof(bd_uint64_t),
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    // flash-size[0]: 0x00000000
    (bd_uint8_t)(BD_Flash_Size>>8), (bd_uint8_t)(BD_Flash_Size>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // flash-size64[0]: 0x0000000000000000
    (bd_uint8_t)(BD_Flash_Size64>>8), (bd_uint8_t)(BD_Flash_Size64>>0), 0x00, sizeof(bd_uint64_t),
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // e2p-size[0]: 0x00000000
    (bd_uint8_t)(BD_Eeeprom_Size>>8), (bd_uint8_t)(BD_Eeeprom_Size>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // nvram-size64[0]: 0x00000000
    (bd_uint8_t)(BD_Nv_Rram_Size>>8), (bd_uint8_t)(BD_Nv_Rram_Size>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // cpu-core-clock[0]: 0x00000000
    (bd_uint8_t)(BD_Cpu_Base_Clk>>8), (bd_uint8_t)(BD_Cpu_Base_Clk>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // cpu-base-clock[0]: 0x00000000
    (bd_uint8_t)(BD_Cpu_Core_Clk>>8), (bd_uint8_t)(BD_Cpu_Core_Clk>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // cpubus-clock[0]: 0x00000000
    (bd_uint8_t)(BD_Cpu_Bus_Clk>>8), (bd_uint8_t)(BD_Cpu_Bus_Clk>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,
    // cpuram-clock[0]: 0x00000000
    (bd_uint8_t)(BD_Ram_Clk>>8), (bd_uint8_t)(BD_Ram_Clk>>0), 0x00, sizeof(bd_uint32_t),
    0x00, 0x00, 0x00, 0x00,

    // partition[0]: (len=0x0F) [ACTIVE, BOOTLOADER, offset=0x00010203, size=0x04050607, 'Part0']
    (bd_uint8_t)(BD_Partition>>8), (bd_uint8_t)(BD_Partition>>0), 0x00, 0x0F,
    BD_Partition_Flags_Active,
    BD_Partition_Type_Raw_BootLoader,
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    'P',  'a',  'r',  't',  '0',
    // partition[0]: (len=0x0F) [INACTIVE, YAFFS2, offset=0x00112233, size=0x44556677, 'Part1']
    (bd_uint8_t)(BD_Partition>>8), (bd_uint8_t)(BD_Partition>>0), 0x00, 2*sizeof(bd_uint8_t)+2*sizeof(bd_uint32_t)+5,
    BD_Partition_Flags_None,
    BD_Partition_Type_FS_YAFFS2,
    0x00, 0x11, 0x22, 0x33,
    0x44, 0x55, 0x66, 0x77,
    'P',  'a',  'r',  't',  '1',
    // partition[0]: (len=0x0F) [ACTIVE, BBT, offset=0xFF00AA55, size=0x00FF55AA, 'Part2']
    (bd_uint8_t)(BD_Partition>>8), (bd_uint8_t)(BD_Partition>>0), 0x00, 2*sizeof(bd_uint8_t)+2*sizeof(bd_uint32_t)+5,
    BD_Partition_Flags_Active,
    BD_Partition_Type_Raw_BBT,
    0xFF, 0x00, 0xAA, 0x55,
    0x00, 0xFF, 0x55, 0xAA,
    'P',  'a',  'r',  't',  '2',
    // partition64[0]: (len=0x0F) [ACTIVE, BBT, ReadOnly, offset=0x0000FFFF'FF00AA55, size=0000FFFF'0x00FF55AA, 'Part3']
    (bd_uint8_t)(BD_Partition64>>8), (bd_uint8_t)(BD_Partition64>>0), 0x00, 8*sizeof(bd_uint8_t)+2*sizeof(bd_uint64_t)+5,
    BD_Partition_Flags_Active,
    BD_Partition_Type_FS_YAFFS2,
    BD_Partition_Opts_ReadOnly,
    0,0,0,0,0,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xAA, 0x55,
    0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x55, 0xAA,
    'P',  'a',  'r',  't',  '3',

    // lcd-type[0]: 0x0000
    (bd_uint8_t)(BD_Lcd_Type>>8), (bd_uint8_t)(BD_Lcd_Type>>0), 0x00, sizeof(bd_uint16_t),
    0x00, 0x00,
    // lcd-backlight[0]: 0x0000
    (bd_uint8_t)(BD_Lcd_Backlight>>8), (bd_uint8_t)(BD_Lcd_Backlight>>0), 0x00, sizeof(bd_uint8_t),
    0x01,
    // lcd-contrast[0]: 0x0000
    (bd_uint8_t)(BD_Lcd_Contrast>>8), (bd_uint8_t)(BD_Lcd_Contrast>>0), 0x00, sizeof(bd_uint8_t),
    0x7F,
    // adapter-type[0]: 0x0000
    BD16(BD_Ui_Adapter_Type), 0x00, sizeof(bd_uint16_t),
    0x00, 0x00,

    // user void[0-15]: -
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,

    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,

    // user row data[0]: 00 01 02 03 04 05 06 07 08 ... 3F
    0x80,  0x01,  0x00, 0x40,
    0x00,  0x01,  0x02,  0x03,  0x04,  0x05,  0x06,  0x07,
    0x08,  0x09,  0x0A,  0x0B,  0x0C,  0x0D,  0x0E,  0x0F,
    0x10,  0x11,  0x12,  0x13,  0x14,  0x15,  0x16,  0x17,
    0x18,  0x19,  0x1A,  0x1B,  0x1C,  0x1D,  0x1E,  0x1F,
    0x20,  0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,
    0x28,  0x29,  0x2A,  0x2B,  0x2C,  0x2D,  0x2E,  0x2F,
    0x30,  0x31,  0x32,  0x33,  0x34,  0x35,  0x36,  0x37,
    0x38,  0x39,  0x3A,  0x3B,  0x3C,  0x3D,  0x3E,  0x3F,

    // end
    (bd_uint8_t)(BD_End>>8), (bd_uint8_t)(BD_End>>0), 0x00, 0x00,
  };
  static const bd_uint8_t hdr1[]   = { 'B', 'D', 'V', '1', BD16(sizeof(data1)), BD16(0x0000) };
  static BD_IndexTable  indexTable[BD_MAX_LENGTH/4];

  BD_Context     bdCtx;
  BD_Entry       bd_entry;
  char           name[128];
  unsigned char  value[BD_MAX_ENTRY_LEN];
  bd_uint_t      num   = 0;
  bd_uint_t      index = 0;
  bd_size_t      len   = 0;
  int            i     = 0;
  BD_Type        type  = BD_Type_None;
  bd_bool_t      rc    = BD_TRUE;

  memset(&bdCtx, 0, sizeof(bdCtx));
  memset(&bd_entry, 0, sizeof(bd_entry));
  memset(indexTable, 0, sizeof(indexTable));
  memset(name, 0, sizeof(name));
  memset(value, 0, sizeof(value));

  /* initialize */
  rc = BD_CheckHeader( &bdCtx, hdr1 );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, data1 );
  BD_ASSERT( rc );
  BD_ASSERT( bdCtx.initialized );
  BD_ASSERT( bdCtx.entries == num_entries );
  rc = BD_InitEntry(&bd_entry);
  BD_ASSERT( rc );

  /* get all bd entries */
  while(BD_GetNextEntry(&bdCtx, &bd_entry)==BD_TRUE) {
    for(i=0;i<sizeof(indexTable)/sizeof(BD_IndexTable);i++) {
      if(indexTable[i].tag == BD_End) {
        index                = 0;
        indexTable[i].tag   = bd_entry.tag;
        indexTable[i].index = 1;
        break;
      } else if(indexTable[i].tag == bd_entry.tag) {
        index = indexTable[i].index;
        indexTable[i].index++;
        break;
      }
    }
    rc=BD_GetInfo(bd_entry.tag, &type, name, sizeof(name) );
    if(rc == BD_FALSE) {
      if(bd_entry.tag == 0x8000) {
        type=BD_Type_Void;
        rc = BD_TRUE;
      } else if(bd_entry.tag == 0x8001) {
        type=BD_Type_None;
        rc = BD_TRUE;
      }
    }
    BD_ASSERT( rc || ((bd_entry.tag==0) && (bd_entry.len==0)) );
    switch(type) {
      case BD_Type_End: {
        BD_ASSERT( (bd_entry.tag==0) && (bd_entry.len==0) && (index==0) );
      } break;
      case BD_Type_Void: {
        rc = BD_GetVoid  (&bdCtx, bd_entry.tag, index, (bd_bool_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag == 0x8000) && (index >= 0) && (index < 16)) {
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_UInt8: {
        rc = BD_GetUInt8 (&bdCtx, bd_entry.tag, index, (bd_uint8_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Hw_Ver) && (index == 0)) {
          BD_ASSERT( *((bd_uint8_t *)value) == 0x01 );
        } else if((bd_entry.tag==BD_Hw_Rel) && (index == 0)) {
          BD_ASSERT( *((bd_uint8_t *)value) == 0x00 );
        } else if((bd_entry.tag==BD_Lcd_Backlight) && (index == 0)) {
          BD_ASSERT( *((bd_uint8_t *)value) == 0x01 );
        } else if((bd_entry.tag==BD_Lcd_Contrast) && (index == 0)) {
          BD_ASSERT( *((bd_uint8_t *)value) == 0x7F );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_UInt16: {
        rc = BD_GetUInt16(&bdCtx, bd_entry.tag, index, (bd_uint16_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Prod_Variant) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0xF0A5 );
        } else if((bd_entry.tag==BD_Prod_Variant) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0xF0A5 );
        } else if((bd_entry.tag==BD_Prod_Variant) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0xF0A5 );
        } else if((bd_entry.tag==BD_Usb_Device_Id) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0xAABB );
        } else if((bd_entry.tag==BD_Usb_Vendor_Id) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0xCCDD );
        } else if((bd_entry.tag==BD_Lcd_Type) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0x0000 );
        } else if((bd_entry.tag==BD_Ui_Adapter_Type) && (index == 0)) {
          BD_ASSERT( *((bd_uint16_t *)value) == 0x0000 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_UInt32: {
        rc = BD_GetUInt32(&bdCtx, bd_entry.tag, index, (bd_uint32_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Ram_Size) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0xA0A1A2A3 );
        } else if((bd_entry.tag==BD_Flash_Size) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Eeeprom_Size) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Nv_Rram_Size) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Cpu_Base_Clk) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Cpu_Core_Clk) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Cpu_Bus_Clk) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else if((bd_entry.tag==BD_Ram_Clk) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == 0x00000000 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_UInt64: {
        rc = BD_GetUInt64(&bdCtx, bd_entry.tag, index, (bd_uint64_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Ram_Size64) && (index == 0)) {
          BD_ASSERT( *(bd_uint64_t*)value == 0xB0B1B2B3B4B5B6B7ULL );
        } else if((bd_entry.tag==BD_Flash_Size64) && (index == 0)) {
          BD_ASSERT( *(bd_uint64_t*)value == 0x0000000000000000ULL );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_String: {
        rc = BD_GetString(&bdCtx, bd_entry.tag, index, (char *)value, bd_entry.len+1);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Prod_Name) && (index == 0)) {
          BD_ASSERT( strcmp((char *)value, "Product") == 0 );
        } else if((bd_entry.tag==BD_Prod_Compatibility) && (index == 0)) {
          BD_ASSERT( strcmp((char *)value, "Comp") == 0 );
        } else if((bd_entry.tag==BD_Serial) && (index == 0)) {
          BD_ASSERT( strcmp((char *)value, "Serial") == 0 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_Date: {
        rc = BD_GetString(&bdCtx, bd_entry.tag, index, (char *)value, bd_entry.len+1);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Production_Date) && (index == 0)) {
          BD_ASSERT( strcmp((char *)value, "01.01.2000") == 0 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
        BD_ASSERT( rc );
      } break;
      case BD_Type_MAC: {
        rc = BD_GetMAC(&bdCtx, bd_entry.tag, index, (bd_uint8_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Eth_Mac) && (index == 0)) {
          BD_ASSERT( value[0] == 0x05 );
          BD_ASSERT( value[1] == 0x14 );
          BD_ASSERT( value[2] == 0x23 );
          BD_ASSERT( value[3] == 0x32 );
          BD_ASSERT( value[4] == 0x41 );
          BD_ASSERT( value[5] == 0x50 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_IPV4: {
        rc = BD_GetIPv4(&bdCtx, bd_entry.tag, index, (bd_uint32_t *)value);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Ip_Addr) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((192<<24)|(168<<16)|(0<<8)|(2<<0)) );
        } else if((bd_entry.tag==BD_Ip_Netmask) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((255<<24)|(255<<16)|(255<<8)|(0<<0)) );
        } else if((bd_entry.tag==BD_Ip_Gateway) && (index == 0)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((192<<24)|(168<<16)|(0<<8)|(1<<0)) );
        } else if((bd_entry.tag==BD_Ip_Addr) && (index == 1)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((172<<24)|(20<<16)|(0<<8)|(2<<0)) );
        } else if((bd_entry.tag==BD_Ip_Netmask) && (index == 1)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((255<<24)|(255<<16)|(0<<8)|(0<<0)) );
        } else if((bd_entry.tag==BD_Ip_Gateway) && (index == 1)) {
          BD_ASSERT( *((bd_uint32_t *)value) == (bd_uint32_t)((172<<24)|(20<<16)|(0<<8)|(1<<0)) );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_Partition: {
        BD_PartitionEntry *pPartition=(BD_PartitionEntry *)value;
        rc = BD_GetPartition(&bdCtx, bd_entry.tag, index, pPartition);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Partition) && (index == 0)) {
          BD_ASSERT( pPartition->flags == BD_Partition_Flags_Active );
          BD_ASSERT( pPartition->type == BD_Partition_Type_Raw_BootLoader );
          BD_ASSERT( pPartition->offset == 0x00010203 );
          BD_ASSERT( pPartition->size == 0x04050607 );
          BD_ASSERT( strcmp(pPartition->name, "Part0") == 0 );
        } else if((bd_entry.tag==BD_Partition) && (index == 1)) {
          BD_ASSERT( pPartition->flags == BD_Partition_Flags_None );
          BD_ASSERT( pPartition->type == BD_Partition_Type_FS_YAFFS2 );
          BD_ASSERT( pPartition->offset == 0x00112233 );
          BD_ASSERT( pPartition->size == 0x44556677 );
          BD_ASSERT( strcmp(pPartition->name, "Part1") == 0 );
        } else if((bd_entry.tag==BD_Partition) && (index == 2)) {
          BD_ASSERT( pPartition->flags == BD_Partition_Flags_Active );
          BD_ASSERT( pPartition->type == BD_Partition_Type_Raw_BBT );
          BD_ASSERT( pPartition->offset == 0xFF00AA55 );
          BD_ASSERT( pPartition->size == 0x00FF55AA );
          BD_ASSERT( strcmp(pPartition->name, "Part2") == 0 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_Partition64: {
        BD_PartitionEntry64 *pPartition64=(BD_PartitionEntry64 *)value;
        rc = BD_GetPartition64(&bdCtx, bd_entry.tag, index, pPartition64);
        BD_ASSERT( rc );
        if((bd_entry.tag==BD_Partition64) && (index == 0)) {
          BD_ASSERT( pPartition64->flags == BD_Partition_Flags_Active );
          BD_ASSERT( pPartition64->type == BD_Partition_Type_FS_YAFFS2 );
          BD_ASSERT( pPartition64->options == BD_Partition_Opts_ReadOnly );
          BD_ASSERT( (pPartition64->offset == 0x0000FFFFFF00AA55ULL) );
          BD_ASSERT( (pPartition64->size == 0x0000FFFF00FF55AAULL) );
          BD_ASSERT( strcmp(pPartition64->name, "Part3") == 0 );
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      case BD_Type_None: {
        rc = BD_GetBlob(&bdCtx, bd_entry.tag, index, (char *)value, bd_entry.len, &len);
        BD_ASSERT( rc );
        BD_ASSERT( bd_entry.len == len );
        if((bd_entry.tag==0x8001) && (index == 0)) {
          BD_ASSERT( len == 0x40 );
          for(i=0;i<(int)len;i++) {
            BD_ASSERT( ((unsigned char)value[i]) == ((unsigned char)i) );
          }
        } else {
          BD_ASSERT( BD_FALSE );
        }
      } break;
      default: {
        BD_ASSERT( rc );
      } break;
    }
    if(type != BD_Type_End) {
      num++;
    }
  }
  BD_ASSERT( bdCtx.entries == num );
}

/*---------------------------------------------------------------------------*/

#ifdef BD_CONF_HAS_HASH

static void bd_testHash( void )
{
  static bd_uint8_t  bdData[] =
  {
    0x42, 0x44, 0x56, 0x31, 0x00, 0x20, 0x04, 0xBE, 0x00, 0x1F, 0x00, 0x06, 0x00, 0x12, 0x63, 0x1E,
    0x22, 0x3D, 0x00, 0x08, 0x00, 0x06, 0x00, 0x11, 0x2B, 0x00, 0xAB, 0xCD, 0x00, 0x09, 0x00, 0x04,
    0xAC, 0x1F, 0x0E, 0xFF, 0x00, 0x00, 0x00, 0x00
  };
  const char  key[]     = "12345";
  const char  keyFail[] = "joshua";
  BD_Context  bdCtx;
  bd_bool_t   rc;


  /* Initialize */
  rc = BD_CheckHeader( &bdCtx, bdData );
  BD_ASSERT( rc );
  rc = BD_ImportData( &bdCtx, bdData+8 );
  BD_ASSERT( rc );

  /* Verify hashed area of board descriptor */
  rc = BD_VerifySha1Hmac( &bdCtx, BD_Hmac_Sha1_4, 0, key, 5);
  BD_ASSERT( rc );

  /* Try with wrong key -> shall fail */
  rc = BD_VerifySha1Hmac( &bdCtx, BD_Hmac_Sha1_4, 0, keyFail, 6);
  BD_ASSERT( !rc );

  /* Now manipulate a byte in the MAC address .. */
  bdData[23]++;
  /* .. and test again. This must fail */
  rc = BD_VerifySha1Hmac( &bdCtx, BD_Hmac_Sha1_4, 0, key, 5);
  BD_ASSERT( !rc );
}

#endif

/*---------------------------------------------------------------------------*/

void BD_UnitTest(void)
{
  bd_testHelpers( );
  bd_testCheckHeader( );
  bd_testImport();
  bd_testChecksum();
  bd_testGetUInt();
  bd_testGetString();
  bd_testGetMAC();
  bd_testGetIPv4();
  bd_testGetPartition();
  bd_testGetEntry();
#ifdef BD_CONF_HAS_HASH
  bd_testHash();
#endif
}

#endif /* BD_CONF_UNIT_TESTS */

/*--- eof -------------------------------------------------------------------*/

