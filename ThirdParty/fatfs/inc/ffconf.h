/*---------------------------------------------------------------------------/
/  FatFs R0.13c — GD32F407 + TF（支持 FAT32 / exFAT）
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	86604

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/
#define FF_FS_READONLY	0
#define FF_FS_MINIMIZE	0
#define FF_USE_STRFUNC	0
#define FF_USE_FIND		0
#define FF_USE_MKFS		0
#define FF_USE_FASTSEEK	0
#define FF_USE_EXPAND	0
#define FF_USE_CHMOD	0
#define FF_USE_LABEL	0
#define FF_USE_FORWARD	0

/*---------------------------------------------------------------------------/
/ Locale / Namespace
/---------------------------------------------------------------------------*/
#define FF_CODE_PAGE	936		/* 简体中文 OEM，配合 Windows 中文文件名 */
#define FF_USE_LFN		1		/* exFAT 必须开 LFN；静态缓冲 */
#define FF_MAX_LFN		128
#define FF_LFN_UNICODE	0		/* API 用当前代码页 TCHAR=char */
#define FF_LFN_BUF		128
#define FF_SFN_BUF		12
#define FF_STRF_ENCODE	0
#define FF_FS_RPATH		0

/*---------------------------------------------------------------------------/
/ Drive / Volume
/---------------------------------------------------------------------------*/
#define FF_VOLUMES		1
#define FF_STR_VOLUME_ID	0
#define FF_MULTI_PARTITION	0
#define FF_MIN_SS		512
#define FF_MAX_SS		512
#define FF_USE_TRIM		0
#define FF_FS_NOFSINFO	0

/*---------------------------------------------------------------------------/
/ System
/---------------------------------------------------------------------------*/
#define FF_FS_TINY		0
#define FF_FS_EXFAT		1		/* 支持大容量 TF 默认 exFAT */
#define FF_FS_NORTC		1
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2026
#define FF_FS_LOCK		0
#define FF_FS_REENTRANT	0
#define FF_FS_TIMEOUT	1000

/*--- End of configuration options ---*/
