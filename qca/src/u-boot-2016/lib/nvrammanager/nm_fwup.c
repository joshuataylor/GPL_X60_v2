/*! Copyright(c) 1996-2009 Shenzhen TP-LINK Technologies Co. Ltd.
 * \file    nm_fwup.c
 * \brief   Implements for upgrade firmware to NVRAM.
 * \author  Meng Qing
 * \version 1.0
 * \date    21/05/2009
 */


/**************************************************************************************************/
/*                                      CONFIGURATIONS                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      INCLUDE_FILES                                             */
/**************************************************************************************************/
#if 0
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "lib_malloc.h"
#endif
#include <common.h>

#include "nm_lib.h"
#include "nm_fwup.h"
#include "nm_api.h"

#include "sysProductInfo.h"

#include <asm/arch-qca-common/smem.h>


/**************************************************************************************************/
/*                                      DEFINES                                                   */
/**************************************************************************************************/
/* Porting memory managing utils. */
extern void *malloc(unsigned int size);
extern void free(void *src);
#define fflush(stdout) 

/**************************************************************************************************/
/*                                      TYPES                                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      EXTERN_PROTOTYPES                                         */
/**************************************************************************************************/
STATUS nm_getDataFromFwupFile(NM_PTN_STRUCT *ptnStruct, char *fwupPtnIndex, char *fwupFileBase);
STATUS nm_getDataFromNvram(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct);
STATUS nm_updateDataToNvram(NM_PTN_STRUCT *ptnStruct);
STATUS nm_updateRuntimePtnTable(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct);
static int nm_checkSupportList(char *support_list, int len);
STATUS nm_checkUpdateContent(NM_PTN_STRUCT *ptnStruct, char *pAppBuf, int nFileBytes, int *errorCode);
STATUS nm_cleanupPtnContentCache(void);
int nm_buildUpgradeStruct(char *pAppBuf, int nFileBytes);
STATUS nm_upgradeFwupFile(char *pAppBuf, int nFileBytes);


/**************************************************************************************************/
/*                                      LOCAL_PROTOTYPES                                          */
/**************************************************************************************************/
static int nm_readFwUpTbl(char *buf, int len);
static NM_UP_PTN_ENTRY *nm_ptnNameToUpEntry(const char *ptnName);
static int nm_writeRootToMtd(char *buf, int len, int idx);
static int nm_eraseRuntimeData(void);
static int nm_setBootIdx(int idx);
static int nm_checkUpContents(char *contentsBuf, int contentSize);


/**************************************************************************************************/
/*                                      VARIABLES                                                 */
/**************************************************************************************************/

NM_STR_MAP nm_fwupPtnIndexFileParaStrMap[] =
{
    {NM_FWUP_PTN_INDEX_PARA_ID_NAME,    "fwup-ptn"},
    {NM_FWUP_PTN_INDEX_PARA_ID_BASE,    "base"},
    {NM_FWUP_PTN_INDEX_PARA_ID_SIZE,    "size"},

    {-1,                                NULL}
};



NM_PTN_STRUCT *g_nmFwupPtnStruct;
NM_PTN_STRUCT g_nmFwupPtnStructEntity;
int g_nmCountFwupCurrWriteBytes;
int g_nmCountFwupAllWriteBytes;

STATUS g_nmUpgradeResult;


char *ptnContentCache[NM_PTN_NUM_MAX];
NM_UP_PTN_TBL_STRUCT g_nmUpTbl;

/* add by yangxv for EMMC, 2016.11 */
#define EMMC_PREFIX "emmc-"

int nm_isEMMCPartition(NM_PTN_ENTRY *entry);
/* end add */


/* add by yangxv for zigbee, 2017.4 */
#define ZB_PREFIX "zigbee-"

int nm_isZBPartition(NM_PTN_ENTRY *entry);
/* end add */



/**************************************************************************************************/
/*                                      LOCAL_FUNCTIONS                                           */
/**************************************************************************************************/
static int nm_readFwUpTbl(char *buf, int len)
{
    int ret = ERROR;
    int index = 0;
    int pre_base = 0;
    int pre_size = 0;
    int fw_size = 0;
    NM_UP_PTN_ENTRY *pEntry = NULL;
    if (len < NM_FWUP_PTN_INDEX_SIZE)
    {
        NM_ERROR("File size too small!");
        return ret;
    }
    memcpy(&g_nmUpTbl, buf, sizeof(NM_UP_PTN_TBL_STRUCT));
    g_nmUpTbl.rootSize = ntohl(g_nmUpTbl.rootSize);
    g_nmUpTbl.ptnNum = ntohl(g_nmUpTbl.ptnNum);
    g_nmUpTbl.isManu = ntohl(g_nmUpTbl.isManu);
    NM_DEBUG("up table: root size: %d, ptn num: %d, is manu %d",
             g_nmUpTbl.rootSize, g_nmUpTbl.ptnNum, g_nmUpTbl.isManu);
    if (g_nmUpTbl.ptnNum > NM_PTN_NUM_MAX)
    {
        NM_ERROR("Too many partitions: %d", g_nmUpTbl.ptnNum);
        return ret;
    }
    pre_base += g_nmUpTbl.rootSize;
    fw_size += g_nmUpTbl.rootSize;
    for (index = 0; index < g_nmUpTbl.ptnNum; index++)
    {
        pEntry = &g_nmUpTbl.entries[index];
        pEntry->base = ntohl(pEntry->base);
        pEntry->size = ntohl(pEntry->size);
        pEntry->content_type = ntohl(pEntry->content_type);
        if (pre_base + pre_size > pEntry->base)
        {
            NM_ERROR("Partition %d is overlapped with before.", index);
            return ret;
        }
        NM_DEBUG("up entry-> name: %s, base: %d, size: %d",
                 pEntry->name, pEntry->base, pEntry->size);
        pre_base = pEntry->base;
        pre_size = pEntry->size;
        fw_size = pre_base + pre_size;
    }
    if (fw_size + NM_FWUP_PTN_INDEX_SIZE > len)
    {
        NM_ERROR("Up file too small, fw_size: %d, len: %d", fw_size, len);
        return ret;
    }
    ret = OK;
    return ret;
}


static NM_UP_PTN_ENTRY *nm_ptnNameToUpEntry(const char *ptnName)
{
    int index = 0;
    if (!ptnName)
    {
        NM_ERROR("null ptnName");
        return NULL;
    }

    for (index = 0; index < g_nmUpTbl.ptnNum; index++)
    {
        if (strncmp(g_nmUpTbl.entries[index].name, ptnName, NM_PTN_NAME_LEN) == 0)
        {
            NM_DEBUG("ptn %s found", ptnName);
            return (&g_nmUpTbl.entries[index]);
        }
    }
    return NULL;
}


static int nm_writeRootToMtd(char *buf, int len, int idx)
{
    qca_smem_flash_info_t *sfi = &qca_smem_flash_info;
    qca_part_entry_t *pEntry = &sfi->rootfs[idx?1:0];
    loff_t mtd_size = pEntry->size;
    NM_DEBUG("Writing MTD len: %d, root idx: %d", len, idx);
    if (mtd_size < len)
    {
        NM_ERROR("MTD size 0x%x is too small for rootfs %d", mtd_size, idx);
        return ERROR;
    }
    if (nm_lib_eraseNandNvram(pEntry->offset, pEntry->size) != OK)
    {
        NM_ERROR("Error erasing 0x%llx@0x%llx", pEntry->size, pEntry->offset);
        return ERROR;
    }
    if (nm_lib_writeNandNvram(buf, pEntry->offset, (loff_t)len, mtd_size) != OK)
    {
        NM_ERROR("Error writing 0x%llx@0x%llx", pEntry->size, (loff_t)len);
        return ERROR;
    }
    return OK;
}



static int nm_eraseRuntimeData()
{
    qca_smem_flash_info_t *sfi = &qca_smem_flash_info;
    qca_part_entry_t *pEntry = &sfi->rt_data;
    NM_DEBUG("Erasing runtime data 0x%llx@0x%llx", pEntry->size, pEntry->offset);
    if(nm_lib_eraseNandNvram(pEntry->offset, pEntry->size) != OK)
    {
        NM_ERROR("Fail erasing 0x%llx@0x%llx", pEntry->size, pEntry->offset);
        return ERROR;
    }
    return OK;
}




static int nm_setBootIdx(int idx)
{
    extern int has_boot_alter_flag;
    NM_DEBUG("Setting boot idx: %d", idx);
    if (setenv(TP_BOOT_IDX, idx ? "1":"0"))
    {
        NM_ERROR("Fail to setenv");
        return ERROR;
    }
    if(saveenv())
    {
        NM_ERROR("Fail to save env");
        return ERROR;
    }
    has_boot_alter_flag = idx ? 1 : 0;
    return OK;
}



static int nm_checkUpContents(char *contentsBuf, int contentSize)
{
    int ret = ERROR;
    int upFileSize = 0;
    int index = 0;
    NM_UP_PTN_ENTRY *pEntry = NULL;

    upFileSize = g_nmUpTbl.rootSize;

    /* file size check */
    for (index = 0; index < g_nmUpTbl.ptnNum; index++)
    {
        pEntry = &g_nmUpTbl.entries[index];
        if (upFileSize != pEntry->base)
        {
            NM_ERROR("Wrong size at ptn %s, index = %d", pEntry->name, index);
            return ret;
        }
        upFileSize += g_nmUpTbl.entries[index].size;
    }

    if (upFileSize > contentSize)
    {
        NM_ERROR("Contents buf's size smaller than firmware");
        return ret;
    }

    /* support list check */
    pEntry = NULL;
    pEntry = nm_ptnNameToUpEntry(NM_PTN_NAME_SUPPORT_LIST);
    if (!pEntry)
    {
        NM_ERROR("%s not found in up tbl", NM_PTN_NAME_SUPPORT_LIST);
        return ret;
    }
    if (nm_checkSupportList(contentsBuf + pEntry->base, pEntry->size))
    {
        NM_ERROR("Fail to check support list");
        return ret;
    }

    ret = OK;
    return ret;
}


/**************************************************************************************************/
/*                                      PUBLIC_FUNCTIONS                                          */
/**************************************************************************************************/

int nm_checkSupportList(char *support_list, int len)
{
    int ret = 0;
   
    PRODUCT_INFO_STRUCT *pProductInfo = NULL;

    /* skip partition header */
    len -= 8;
    support_list += 8;
 
    /* check list prefix string */
    if (len < 12 || strncmp(support_list, "SupportList:", 12) != 0)
        return 0;

    len -= 12;
    support_list += 12;

    pProductInfo = sysmgr_getProductInfo();
    ret = sysmgr_cfg_checkSupportList(pProductInfo, support_list, len);
    if (0 == ret)
    {
        NM_INFO("Firmwave supports, check OK.\r\n");
        return 1;
    }
    
    NM_INFO("Firmwave not supports, check failed.\r\n");
    return 0;
}

STATUS nm_checkPartition(NM_PTN_STRUCT *ptnStruct, const char *name, int *errorCode)
{
    int ptnFound = FALSE;
    NM_PTN_ENTRY *currPtnEntry = NULL;
    int index;

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);

        if (strncmp(currPtnEntry->name, name, NM_PTN_NAME_LEN) == 0)
        {
            ptnFound = TRUE;
            break;
        }
    }
    if (ptnFound == FALSE)
    {
        NM_ERROR("ptn \"%s\" not found whether in fwup-file or NVRAM.", name);
        *errorCode = -NM_FWUP_ERROR_BAD_FILE;
        return ERROR;
    }

    return OK;
}



/*******************************************************************
 * Name		: nm_buildUpgradeStruct
 * Abstract	: Generate an upgrade file from NVRAM and firmware file.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR.
 */
int nm_buildUpgradeStruct(char *pAppBuf, int nFileBytes)
{
    char *basicAddr = NULL;
    if (nm_readFwUpTbl(pAppBuf, nFileBytes) != OK)
    {
        NM_ERROR("Fail to read up tbl");
        return ERROR;
    }

    basicAddr = pAppBuf + NM_FWUP_PTN_INDEX_SIZE;
    if (nm_checkUpContents(basicAddr, nFileBytes) != OK)
    {
        NM_ERROR("checkUpdateContent failed.");
        return ERROR;
    }
    return OK;
}


/*******************************************************************
 * Name		: nm_upgradeFwupFile
 * Abstract	: upgrade the FwupFile to NVRAM
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR.
 */
STATUS nm_upgradeFwupFile(char *pAppBuf, int nFileBytes)
{   
    g_nmUpgradeResult = FALSE;

    extern int has_boot_alter_flag;
    int idx = has_boot_alter_flag ? 0 : 1;
    char *basic_addr = pAppBuf + NM_FWUP_PTN_INDEX_SIZE;
    if (nm_writeRootToMtd(basic_addr, g_nmUpTbl.rootSize, idx) != OK)
    {
        NM_ERROR("Fail to write root to mtd as rootfs %d", idx);
        goto leave;
    }
    if (g_nmUpTbl.isManu)
    {
        NM_DEBUG("Firmware is manu fw, will erase runtimeData.");
        if (nm_eraseRuntimeData() != OK)
        {
            NM_ERROR("Fail to erase runtime data");
            goto leave;
        }
    }
    if (nm_setBootIdx(idx) != OK)
    {
        NM_ERROR("Fail to set boot idx.");
        goto leave;
    }

    g_nmUpgradeResult = TRUE;
    return OK;
    
leave:
    g_nmUpgradeResult = FALSE;
    return ERROR;
}


#define IMAGE_SIZE_LEN  (0x04)
#define IMAGE_SIZE_MD5  (0x10)
#define IMAGE_SIZE_PRODUCT  (0x1000)
#define IMAGE_SIZE_BASE (IMAGE_SIZE_LEN + IMAGE_SIZE_MD5 + IMAGE_SIZE_PRODUCT)

#define IMAGE_SIZE_MAX  (IMAGE_SIZE_BASE + 0x800 + NM_FLASH_SIZE)
#define IMAGE_SIZE_MIN  (IMAGE_SIZE_BASE + 0x800)

#if 1

#define IMAGE_SIZE_FWTYPE	(IMAGE_SIZE_LEN + IMAGE_SIZE_MD5)
#define IMAGE_SIZE_RSA_SIG	(IMAGE_SIZE_FWTYPE + 0x11C)
#define IMAGE_LEN_RSA_SIG	0x80


static unsigned char l_rsaPubKey[] = "BgIAAACkAABSU0ExAAQAAAEAAQD9lxDCQ5DFNSYJBriTmTmZlEMYVgGcZTO+AIwm" \
				"dVjhaeJI6wWtN7DqCaHQlOqJ2xvKNrLB+wA1NxUh7VDViymotq/+9QDf7qEtJHmesji" \
				"rvPN6Hfrf+FO4/hmjbVXgytHORxGta5KW4QHVIwyMSVPOvMC4A5lFIh+D1kJW5GXWtA==";
static int handle_fw_cloud(unsigned char *buf, int buf_len);


static int handle_fw_cloud(unsigned char *buf, int buf_len)
{	
	unsigned char md5_dig[IMAGE_SIZE_MD5] = {0};
	unsigned char sig_buf[IMAGE_LEN_RSA_SIG] = {0};
	unsigned char tmp_rsa_sig[IMAGE_LEN_RSA_SIG] = {0};
	int ret = 0;

	/*backup data*/
	memcpy(tmp_rsa_sig,buf + IMAGE_SIZE_RSA_SIG, IMAGE_LEN_RSA_SIG);
	memcpy(sig_buf, buf + IMAGE_SIZE_RSA_SIG, IMAGE_LEN_RSA_SIG);

	/* fill with 0x0 */
	memset(buf + IMAGE_SIZE_RSA_SIG, 0x0, IMAGE_LEN_RSA_SIG);

	md5(buf + IMAGE_SIZE_FWTYPE, buf_len - IMAGE_SIZE_FWTYPE, md5_dig);

	ret = rsaVerifySignByBase64EncodePublicKeyBlob(l_rsaPubKey, strlen(l_rsaPubKey),
                md5_dig, IMAGE_SIZE_MD5, sig_buf, IMAGE_LEN_RSA_SIG);

	memcpy(buf + IMAGE_SIZE_RSA_SIG, tmp_rsa_sig, IMAGE_LEN_RSA_SIG);
	
	return ret;
}
#endif

int nm_upgradeFirmware(char *buf, int size)
{
    int filesize = 0;

    memcpy(&filesize, buf, sizeof(int));
    filesize = ntohl(filesize);

    if (filesize < IMAGE_SIZE_MIN || filesize > IMAGE_SIZE_MAX || filesize > size)
    {
        NM_ERROR("Bad file size: buffer: %d, file: %d", size, filesize);
        return -1;
    }
#if 1
	if (!handle_fw_cloud((unsigned char*)buf, size))
	{
		NM_ERROR("Verify sig error!\n");
		return -1;
	}
#endif

    buf += IMAGE_SIZE_BASE;
    size -= IMAGE_SIZE_BASE;
    if (nm_buildUpgradeStruct(buf, size) != 0)
    {
        NM_ERROR("Firmware is invalid");
        return -1;
    }

    printf("Firmware checking passed\n");

    if (nm_upgradeFwupFile(buf, size) != 0)
    {
        NM_ERROR("Failed to upgrade");
        return -1;
    }

    return 0;
}



/**************************************************************************************************/
/*                                      GLOBAL_FUNCTIONS                                          */
/**************************************************************************************************/

