
/*! Copyright(c) 1996-2009 Shenzhen TP-LINK Technologies Co. Ltd.
 * \file    nm_lib.c
 * \brief   library functions for NVRAM manager.
 * \author  Meng Qing
 * \version 1.0
 * \date    24/04/2009
 */

/**************************************************************************************************/
/*                                      CONFIGURATIONS                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      INCLUDE_FILES                                             */
/**************************************************************************************************/
#if 0
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"

#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#endif
#include <common.h>

#include "nm_lib.h"
#include "nm_fwup.h"
#include "nm_api.h"
#include <nand.h>



/**************************************************************************************************/
/*                                      DEFINES                                                   */
/**************************************************************************************************/
#define FLASH_SECTOR_SIZE      (64*1024)
#define FLASH_DEV_NAME         "flash0"

#define copy_from_user(kernel, user, len) memcpy(kernel, user, len)
#define copy_to_user(user, kernel, len)   memcpy(user, kernel, len)
extern unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base);
#define strtoul(a,b,c)                    simple_strtoul(a, b, c)
/**************************************************************************************************/
/*                                      TYPES                                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      EXTERN_PROTOTYPES                                         */
/**************************************************************************************************/
//extern unsigned long bcm_strtoul(const char *cp, char **endp, unsigned int base);

/**************************************************************************************************/
/*                                      LOCAL_PROTOTYPES                                          */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      VARIABLES                                                 */
/**************************************************************************************************/
static int  l_nminited = FALSE;
NM_PTN_STRUCT *g_nmPtnStruct;
NM_PTN_STRUCT g_nmPtnStructEntity;

#if USE_LOCK
SEM_ID g_nmReadWriteLock;
#endif

NM_STR_MAP nm_ptnIndexFileParaStrMap[] =
{
    {NM_PTN_INDEX_PARA_ID_NAME,         "partition"},
    {NM_PTN_INDEX_PARA_ID_BASE,         "base"},
    {NM_PTN_INDEX_PARA_ID_TAIL,         "tail"},
    {NM_PTN_INDEX_PARA_ID_SIZE,         "size"},

    {-1,                                NULL}
};

static unsigned char nvrammngr_rwBuf[2*FLASH_SECTOR_SIZE];

/**************************************************************************************************/
/*                                      LOCAL_FUNCTIONS                                           */
/**************************************************************************************************/
static int _nvrammng_flash_erase(unsigned int off)
{
    nm_flashOpPortErase(off, FLASH_SECTOR_SIZE);
    return 0;
}

static int _nvrammngr_flash_read(unsigned char *in, unsigned int off, unsigned int len)
{
    nm_flashOpPortRead(off, in, len);
    return 0;
}

static int _nvrammngr_flash_write(unsigned int off, unsigned int len, unsigned char *in)
{
    nm_flashOpPortWrite(off, in, len);
    return 0;
}

int nm_lib_writePtntoNvram_unify(uint32_t hasHead, uint32_t offset, uint8_t *data, uint32_t len)
{
	uint32_t address = 0;
	uint32_t headLen = 0;
	uint32_t endAddr = 0, startAddr = 0;
	uint8_t *orignData = NULL;
	uint32_t headData[2] = {len, 0};
	uint32_t frontLen = 0, tailLen = 0;
	uint32_t retLen = 0;

	headData[0] = htonl(len);	

	if (hasHead != 0)
	{
		headLen = 2 * sizeof(uint32_t);
		len += headLen;
	}

	frontLen = offset % FLASH_SECTOR_SIZE;
	tailLen  = (offset + len) % FLASH_SECTOR_SIZE;
	address = offset - frontLen;
	endAddr = offset + len - tailLen;

	orignData = (uint8_t *)(nvrammngr_rwBuf + FLASH_SECTOR_SIZE);

	if (frontLen > 0 || headLen > 0)
	{
		retLen = _nvrammngr_flash_read(orignData, address, FLASH_SECTOR_SIZE);
		memcpy(nvrammngr_rwBuf, orignData, frontLen);
		if (FLASH_SECTOR_SIZE < frontLen + headLen) 
		{
			headLen = FLASH_SECTOR_SIZE - frontLen;
			memcpy(nvrammngr_rwBuf + frontLen, headData, headLen);

			/***************************************************/
			if (memcmp(orignData, nvrammngr_rwBuf, FLASH_SECTOR_SIZE))
			{
				_nvrammng_flash_erase(address);
				retLen = _nvrammngr_flash_write(address, FLASH_SECTOR_SIZE, nvrammngr_rwBuf);
			}
			address += FLASH_SECTOR_SIZE;
			/***************************************************/
			retLen = _nvrammngr_flash_read(orignData, address, FLASH_SECTOR_SIZE);
			memcpy(nvrammngr_rwBuf, (uint8_t*)(headData) + headLen, 8 - headLen);

			if (len - headLen < FLASH_SECTOR_SIZE) 
			{
				headLen = 8 - headLen;
				copy_from_user(nvrammngr_rwBuf + headLen, data, tailLen - headLen);
				memcpy(nvrammngr_rwBuf + tailLen, orignData + tailLen, FLASH_SECTOR_SIZE - tailLen);
				data += tailLen - headLen;
			}
			else
			{
				headLen = 8 - headLen;
				copy_from_user(nvrammngr_rwBuf + headLen, data, FLASH_SECTOR_SIZE - headLen);
				data += FLASH_SECTOR_SIZE - headLen;
			}
		}
		else
		{
			memcpy(nvrammngr_rwBuf + frontLen, headData, headLen);
			
			if (len + frontLen < FLASH_SECTOR_SIZE)
			{
				copy_from_user(nvrammngr_rwBuf + frontLen + headLen, data, len - headLen);
				data += len - headLen;
				memcpy(nvrammngr_rwBuf + frontLen + len,
						orignData + frontLen + len,
						FLASH_SECTOR_SIZE - (frontLen + len));
			}
			else
			{
				copy_from_user(nvrammngr_rwBuf + frontLen + headLen, data, FLASH_SECTOR_SIZE - frontLen - headLen);
				data += FLASH_SECTOR_SIZE - frontLen - headLen;
			}
		}

		/***************************************************/
		if (memcmp(orignData, nvrammngr_rwBuf, FLASH_SECTOR_SIZE))
		{
			_nvrammng_flash_erase(address);
			retLen = _nvrammngr_flash_write(address, FLASH_SECTOR_SIZE, nvrammngr_rwBuf);
		}
		address += FLASH_SECTOR_SIZE;
		/***************************************************/
	}

	if (address < endAddr)
	{
		startAddr = address;
		while (address < endAddr)
		{
			retLen = _nvrammngr_flash_read(orignData, address, FLASH_SECTOR_SIZE);
			copy_from_user(nvrammngr_rwBuf, data, FLASH_SECTOR_SIZE);
			/***************************************************/
			if (memcmp(orignData, nvrammngr_rwBuf, FLASH_SECTOR_SIZE))
			{
				_nvrammng_flash_erase(address);
				retLen = _nvrammngr_flash_write(address, FLASH_SECTOR_SIZE, nvrammngr_rwBuf);
			}
			address += FLASH_SECTOR_SIZE;
			/***************************************************/
			data += FLASH_SECTOR_SIZE;
		}
	}

	if (address < offset + len) 
	{
		retLen = _nvrammngr_flash_read(orignData, address, FLASH_SECTOR_SIZE);
		copy_from_user(nvrammngr_rwBuf, data, tailLen);
		memcpy(nvrammngr_rwBuf + tailLen, orignData + tailLen, FLASH_SECTOR_SIZE - tailLen);
		/***************************************************/
		if (memcmp(orignData, nvrammngr_rwBuf, FLASH_SECTOR_SIZE)) 
		{
			_nvrammng_flash_erase(address);
			retLen = _nvrammngr_flash_write(address, FLASH_SECTOR_SIZE, nvrammngr_rwBuf);
		}
		address += FLASH_SECTOR_SIZE;
		/***************************************************/
	}

	return 0;
}

static int nm_lib_readFlash_unify(uint32_t offset, uint8_t *usr_buf, uint32_t len)
{
	uint32_t startBlkAddr = 0, endBlkAddr = 0;
	uint32_t frontLen = 0, tailLen = 0;
    uint32_t readAddr = 0;
	uint32_t retLen = 0;

    uint8_t *pUsrBufCur = usr_buf;

	frontLen     = offset % FLASH_SECTOR_SIZE;
	tailLen      = (offset + len) % FLASH_SECTOR_SIZE;
	startBlkAddr = offset - frontLen;
	endBlkAddr   = offset + len - tailLen;
    readAddr     = startBlkAddr;

    //printk("[NVRAM-MNGR] read frontLen %d \n", frontLen);
    //printk("[NVRAM-MNGR] read tailLen %d \n",  tailLen);
    //printk("[NVRAM-MNGR] read startBlkAddr %d \n", startBlkAddr);
    //printk("[NVRAM-MNGR] read endBlkAddr %d \n", endBlkAddr);
    //printk("[NVRAM-MNGR] read frontreadAddrLen %d \n", readAddr);
    //printk("[NVRAM-MNGR] read user_buf addr %08x \n", usr_buf);

	if (frontLen > 0)
	{
        retLen = _nvrammngr_flash_read(nvrammngr_rwBuf, readAddr, FLASH_SECTOR_SIZE);
        if (startBlkAddr == endBlkAddr)  /* read in one block */
        {
            copy_to_user(pUsrBufCur, nvrammngr_rwBuf+frontLen, len);
        }
        else /* read across a block */
        {
    		copy_to_user(pUsrBufCur, nvrammngr_rwBuf+frontLen, FLASH_SECTOR_SIZE-frontLen);
            pUsrBufCur = pUsrBufCur + FLASH_SECTOR_SIZE - frontLen;
        }   
        readAddr += FLASH_SECTOR_SIZE;         
    }
		
	if (startBlkAddr < endBlkAddr) /* blocks in middle */
	{
		while (readAddr < endBlkAddr)
		{
			retLen = _nvrammngr_flash_read(nvrammngr_rwBuf, readAddr, FLASH_SECTOR_SIZE);
			copy_to_user(pUsrBufCur, nvrammngr_rwBuf, FLASH_SECTOR_SIZE);
            pUsrBufCur += FLASH_SECTOR_SIZE;
			readAddr += FLASH_SECTOR_SIZE;
		}
	}

	if (readAddr < offset + len) 
	{
		retLen = _nvrammngr_flash_read(nvrammngr_rwBuf, readAddr, FLASH_SECTOR_SIZE);
        //printk("[NVRAM-MNGR] Dumping nvrammngr_rwBuf before copy to user\n");        
        //hexdump(nvrammngr_rwBuf, 64, (int)nvrammngr_rwBuf);
        //printk("[NVRAM-MNGR] copy to user space addr %08x, data len %d \n", pUsrBufCur, tailLen);
		copy_to_user(pUsrBufCur, nvrammngr_rwBuf, tailLen);
	}

	return 0;
}


/**************************************************************************************************/
/*                                      PUBLIC_FUNCTIONS                                          */
/**************************************************************************************************/

/*******************************************************************
 * Name		: nm_lib_parseU32
 * Abstract	: Converts the string in arg to numeric value.
 * Input	: 
 * Output	: 
 * Return	: success:    0.
 *            fail:       -1
 */
int nm_lib_parseU32(NM_UINT32 *val, const char *arg)
{
    unsigned long res;
    char *ptr = NULL;

    if (!arg || !*arg)
    {
        return -1;
    }

    res = strtoul(arg, &ptr, 0);
    if (!ptr || ptr == arg || *ptr || res > 0xFFFFFFFFUL)
    {
        return -1;
    }
    *val = res;

    
    return 0;
}



/*******************************************************************
 * Name		: nm_lib_makeArgs
 * Abstract	: parse the string of partition-table.
 * Input	: 
 * Output	: 
 * Return	: 
 */
int nm_lib_makeArgs(char *string, char *argv[], int maxArgs)
{
    static const char ws[] = " \t\r\n";
    char *cp;
    int argc = 0;
    char *p_last = NULL;
    
    if (string == NULL)
    {
        return -1;
    }

    for (cp = strtok_r(string, ws, &p_last); cp; cp = strtok_r(NULL, ws, &p_last)) 
    {
        if (argc >= (maxArgs - 1)) 
        {
            NM_ERROR("Too many arguments.");
            return -1;
        }
        argv[argc++] = cp;
    }
    argv[argc] = NULL;

    return argc;
}



/*******************************************************************
 * Name		: nm_lib_strToKey
 * Abstract	: Converts the string in map to numeric value.
 * Input	: map:    The int to string map to use.
 *			  str:    The string representation.
 * Output	: 
 * Return	: success:    The key representation.
 *            fail:       -1
 */
int nm_lib_strToKey(NM_STR_MAP *map, char *str)
{
    int index;

    if (str)
    {
        for (index=0; map[index].str != NULL; index++)
        {
            NM_DEBUG("str(%s) ?= map(%s)", str, map[index].str);
            if (strcmp(str, map[index].str) == 0)
            {
                return map[index].key;
            }
        }
    }
    
    return -1;
}


/*******************************************************************
 * Name		: nm_lib_ptnNameToEntry
 * Abstract	: get partition-entry match the input name.
 * Input	: 
 * Output	: 
 * Return	: point to the partition-entry if match successful.
 *            NULL if match failed.
 */
NM_PTN_ENTRY *nm_lib_ptnNameToEntry(NM_PTN_STRUCT *ptnStruct, char *name)
{
    int index;

    if ((ptnStruct == NULL) || (name == NULL))
    {
        NM_ERROR("invalid input param.");
        return NULL;
    }

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {       
        if (strcmp(ptnStruct->entries[index].name, name) == 0)
            return &(ptnStruct->entries[index]);
    }
    
    return NULL;
}


/*******************************************************************
 * Name		: nm_lib_fetchUnusedPtnEntry
 * Abstract	: get an unused partition-entry from partition-struct.
 * Input	: 
 * Output	: 
 * Return	: point to the partition-entry if match successful.
 *            NULL if match failed.
 */
NM_PTN_ENTRY *nm_lib_fetchUnusedPtnEntry(NM_PTN_STRUCT *ptnStruct)
{
    int index;

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {       
        if (ptnStruct->entries[index].usedFlag != TRUE)
        {
            ptnStruct->entries[index].usedFlag = TRUE;
            return &(ptnStruct->entries[index]);
        }
    }
    
    return NULL;
}


/*******************************************************************
 * Name		: nm_lib_writeHeadlessPtnToNvram
 * Abstract	: write the value of a partition in NVRAM.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR
 */
int nm_lib_writeHeadlessPtnToNvram(char *base, char *buf, int len)
{
    NM_DEBUG("ptnEntry->base = %08x, buf = %08x, len = %d", base + NM_NVRAM_BASE, buf, len);

    if (nm_lib_writePtntoNvram_unify(0, (uint32_t)base + NM_NVRAM_BASE, (uint8_t*)buf, len) < 0)
    {
        return -1;
    }
    
    return len;
}


/*******************************************************************
 * Name		: nm_lib_writePtnToNvram
 * Abstract	: write the value of a partition in NVRAM.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR
 */
int nm_lib_writePtnToNvram(char *base, char *buf, int len)
{
    NM_DEBUG("ptnEntry->base = %08x, buf = %08x, len = %d", base + NM_NVRAM_BASE, buf, len);
	
    if (nm_lib_writePtntoNvram_unify(1, (uint32_t)base + NM_NVRAM_BASE, (uint8_t*)buf, len) < 0)
    {
        return -1;
    }

    return len;
}


/*******************************************************************
 * Name		: nm_lib_readHeadlessPtnFromNvram
 * Abstract	: read the value of a partition in NVRAM.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR
 */
int nm_lib_readHeadlessPtnFromNvram(char *base, char *buf, int len)
{
    
    if (nm_lib_readFlash_unify((uint32_t)base + NM_NVRAM_BASE, (uint8_t*)buf, len) < 0)
    {
        return -1;
    }
    
    return len;
}



/*******************************************************************
 * Name		: nm_lib_readPtnFromNvram
 * Abstract	: read the value of a partition in NVRAM.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR
 */
int nm_lib_readPtnFromNvram(char *base, char *buf, int len)
{
    int ret = OK;
	uint32_t partition_used_len = 0;
    uint32_t read_len = 0;

    uint32_t offset = (uint32_t)base;

	ret = nm_lib_readFlash_unify(offset, (unsigned char*)&partition_used_len, sizeof(uint32_t));
    if (ret < 0)
    {
        NM_ERROR("Read Partition used size failed(%d).", ret);
        ret = -1;
        goto leave;
    }

	partition_used_len = ntohl(partition_used_len);
	read_len = (len > partition_used_len) ? partition_used_len : len;

    NM_DEBUG("partition_used_len = %d, requried len = %d", partition_used_len, len);

	/* jump over partition length and checksum */
    ret = nm_lib_readFlash_unify(offset + sizeof(int) + sizeof(int), (unsigned char*)buf, read_len);
	if (ret < 0)
	{
        NM_ERROR("Read Partition data failed(%d).", ret);
        ret = -2;
        goto leave;
	}

leave:
    return ret;
}



int nm_lib_eraseNandNvram(loff_t off, loff_t size)
{
    int dev = nand_curr_device;
    nand_info_t *nand = &nand_info[dev];
    nand_erase_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.offset = off;
    opts.length = size;
    NM_DEBUG("Erasing nand 0x%llx@0x%llx", size, off);
    if (nand_erase_opts(nand, &opts))
    {
        NM_ERROR("Fail to erase nand flash. %llx@%llx",
                 (unsigned long long)size, (unsigned long long)off);
        return ERROR;
    }
    return OK;
}



int nm_lib_writeNandNvram(char *buf, loff_t off, loff_t len, loff_t maxsize)
{
    int dev = nand_curr_device;
    nand_info_t *nand = &nand_info[dev];
    size_t size = len;
    if (len > maxsize)
    {
        NM_ERROR("Size exceeds partition or device limit.");
        return ERROR;
    }
    NM_DEBUG("Writing offset 0x%llx, size 0x%llx", (unsigned long long)off, (unsigned long long)size);
    if (nand_write_skip_bad(nand, off, &size, NULL, maxsize, (u_char *)buf, WITH_WR_VERIFY))
    {
        NM_ERROR("Fail to write to target flash erea.");
        return ERROR;
    }
    return OK;
}



/**************************************************************************************************/
/*                                      GLOBAL_FUNCTIONS                                          */
/**************************************************************************************************/

