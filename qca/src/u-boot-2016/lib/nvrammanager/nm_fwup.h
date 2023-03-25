
/*! Copyright(c) 1996-2009 Shenzhen TP-LINK Technologies Co. Ltd.
 * \file    ipifcfg_cfg.h
 * \brief   Protos for ipnet ifconfig's config mod.
 * \author  Meng Qing
 * \version 1.0
 * \date    21/1/2009
 */

#ifndef NM_FWUP_H
#define NM_FWUP_H


#ifdef __cplusplus
extern "C"{
#endif


/**************************************************************************************************/
/*                                      CONFIGURATIONS                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      INCLUDE_FILES                                             */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      DEFINES                                                   */
/**************************************************************************************************/
#define NM_FWUP_FRAGMENT_SIZE 0x10000
#define NM_FWUP_SUPPORT_LIST_NUM_MAX 16


/**************************************************************************************************/
/*                                      TYPES                                                     */
/**************************************************************************************************/
typedef int STATUS;

typedef enum _FWUP_ERROR_TYPE
{
    NM_FWUP_ERROR_NORMAL = 1, 
    NM_FWUP_ERROR_INVALID_FILE,         /* invalid firmware file */
    NM_FWUP_ERROR_INCORRECT_MODEL,      /* the firmware is not for this model */
    NM_FWUP_ERROR_BAD_FILE,             /* an valid firmware, but something wrong in file */
    NM_FWUP_ERROR_UNSUPPORT_VER,    	/* firmware file not compatible with current version */
} FWUP_ERROR_TYPE;


/**************************************************************************************************/
/*                                      FUNCTIONS                                                 */
/**************************************************************************************************/

extern STATUS nm_upgradeFwupFile(char *pAppBuf, int nFileBytes);
extern int nm_buildUpgradeStruct(char *pAppBuf, int nFileBytes);
extern int nm_upgradeFirmware(char *buf, int size);

/**************************************************************************************************/
/*                                      VARIABLES                                                 */
/**************************************************************************************************/

extern int g_nmCountFwupAllWriteBytes;
extern int g_nmCountFwupCurrWriteBytes;
extern int g_nmUpgradeResult;




#ifdef __cplusplus 
}
#endif

#endif


