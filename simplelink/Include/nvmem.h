/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
*
*   All rights reserved. Property of Texas Instruments Incorporated.
*   Restricted rights to use, duplicate or disclose this code are
*   granted through contract.
*
*   The program may not be used without the written permission of
*   Texas Instruments Incorporated or against the terms and conditions
*   stipulated in the agreement under which this program has been supplied,
*   and under no circumstances can it be used with non-TI connectivity device.
*
******************************************************************************/
    
#include "simplelink.h"

#ifndef __NVMEM_H__
#define	__NVMEM_H__

#ifdef	__cplusplus
extern "C" {
#endif


typedef struct
{
    unsigned short flags;
    unsigned long  FileLen;
    unsigned long  AllocatedLen;
    unsigned long  Token[3];

}SlFsFileInfo_t;

typedef enum
{
   _FS_MODE_OPEN_READ,
   _FS_MODE_OPEN_WRITE,
   _FS_MODE_OPEN_CREATE, 
   _FS_MODE_OPEN_CREATE_NO_FAIL_SAFE
}SlFsFileAccessType_e;


typedef enum
{
       _FS_MODE_SIZE_GRAN_256B    = 0,   // MAX_SIZE = 64K
       _FS_MODE_SIZE_GRAN_1KB,           // MAX_SIZE = 256K
       _FS_MODE_SIZE_GRAN_4KB,           // MAX_SZIE = 1M
       _FS_MODE_SIZE_GRAN_16KB,          // MAX_SIZE = 4M
       _FS_MODE_SIZE_GRAN_64KB,          // MAX_SIZE = 16M
       _FS_MAX_MODE_SIZE_GRAN
}SlFsFileOpenMaxSizeGran_e;


#define _FS_MODE_ACCESS_OFFSET                       12
#define _FS_MODE_ACCESS_MASK                         0xF
#define _FS_MODE_OPEN_SIZE_GRAN_OFFSET               8
#define _FS_MODE_OPEN_SIZE_GRAN_MASK                 0xF
#define _FS_MODE_OPEN_SIZE_OFFSET                    0
#define _FS_MODE_OPEN_SIZE_MASK                      0xFF

#define MAX_MODE_SIZE                                0xFF

//Access of type SlFsFileOpenMaxSizeGran_e
//SizeGran of type FsFileOpenMaxSizeGran_e
#define _FS_MODE(Access, SizeGran, Size)        (UINT32)((((Access) & _FS_MODE_ACCESS_MASK)<<_FS_MODE_ACCESS_OFFSET) | (((SizeGran) & _FS_MODE_OPEN_SIZE_GRAN_MASK)<<_FS_MODE_OPEN_SIZE_GRAN_OFFSET) | (((Size) & _FS_MODE_OPEN_SIZE_MASK)<<_FS_MODE_OPEN_SIZE_OFFSET))

// sl_FsOpen options ////

// Open for Read 
#define FS_MODE_FILE_OPEN_READ                         _FS_MODE(_FS_MODE_OPEN_READ, 0, 0)
// Open for Write (in case file exist)
#define FS_MODE_OPEN_WRITE                             _FS_MODE(_FS_MODE_OPEN_WRITE, 0, 0)
// Open for Creating a file with fail safe on/off and max size   
#define FS_MODE_CREATE_MAX_SIZE_4K                     _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 1)
#define FS_MODE_CREATE_MAX_SIZE_8K                     _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 2)
#define FS_MODE_CREATE_MAX_SIZE_16K                    _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 4)
#define FS_MODE_CREATE_MAX_SIZE_64K                    _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 16)
#define FS_MODE_CREATE_MAX_SIZE_256K                   _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 64)
#define FS_MODE_CREATE_MAX_SIZE_512K                   _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 128)
#define FS_MODE_CREATE_MAX_SIZE_1M                     _FS_MODE(_FS_MODE_OPEN_CREATE, _FS_MODE_SIZE_GRAN_4KB, 256)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_4K        _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB,   1)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_8K        _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB,   2)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_16K       _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB,   4)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_64K       _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB,  16)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_256K      _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB,  64)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_512K      _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB, 128)
#define FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_1M        _FS_MODE(_FS_MODE_OPEN_CREATE_NO_FAIL_SAFE, _FS_MODE_SIZE_GRAN_4KB, 256)



/*!

    \addtogroup nvmem
    @{

*/

/*!
    \brief open file for read or write from/to storage device
    
    \param[in]      pFileName                  File Name buffer pointer  
    \param[in]      AccessModeAndMaxSize       Options: As described below
    \param[in]      pToken                     input Token for read, output Token for write
    \param[out]     pFileHandle      Pointing on the file and used for read and write commands to the file     
     
     AccessModeAndMaxSize possible input                                                                        \n
     FS_MODE_FILE_OPEN_READ      - read a file                                                                  \n
	 FS_MODE_OPEN_WRITE          - open for write for an existing file                                          \n
     FS_MODE_CREATE_MAX_SIZE_4K  - write a file, max size 4Kb, fail safe                                        \n
     FS_MODE_CREATE_MAX_SIZE_8K                                                                                 \n
     FS_MODE_CREATE_MAX_SIZE_16K                                                                                \n
     FS_MODE_CREATE_MAX_SIZE_64K                                                                                \n
     FS_MODE_CREATE_MAX_SIZE_256K                                                                               \n
     FS_MODE_CREATE_MAX_SIZE_512K                                                                               \n
     FS_MODE_CREATE_MAX_SIZE_1M                                                                                 \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_4K  - write a file, max size 4Kb, not fail safe                       \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_8K                                                                    \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_16K                                                                   \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_64K                                                                   \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_256K                                                                  \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_512K                                                                  \n
     FS_MODE_CREATE_MAX_SIZE_NO_FAIL_SAFE_1M                                                                    \n

    \return         On success, zero is returned. On error, negative is returned    
    
    \sa             sl_FsRead sl_FsWrite sl_FsClose       
    \note           belongs to \ref basic_api       
    \warning        This API is going to be modified on the next release
	\par            Example:
	\code			
	sl_FsOpen("FileName.html",FS_MODE_FILE_OPEN_READ,&Token, &FileHandle);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsOpen)
long sl_FsOpen(unsigned char *pFileName,unsigned long AccessModeAndMaxSize, unsigned long *pToken,long *pFileHandle);
#endif

/*!
    \brief close file in storage device
    
    \param[in]      FileHdl   Pointer to the file (assigned from sl_FsOpen) 
    \param[in]      CertificateFileId
    \param[in]      SignatureLen
    \param[in]      Signature 
    \return         On success, zero is returned. On error, negative is returned    
    
    \sa             sl_FsRead sl_FsWrite sl_FsOpen        
    \note           belongs to \ref basic_api       
    \warning
	\par            Example:
	\code			
	sl_FsClose(FileHandle,0,0,0);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsClose)
int sl_FsClose(long FileHdl,unsigned char CertificateFileId,unsigned char SignatureLen,unsigned char Signature);
#endif

/*!
    \brief read block of data from a file in storage device
    
    \param[in]      FileHdl Pointer to the file (assigned from sl_FsOpen)    
    \param[in]      Offset  Offset to specific read block
    \param[out]     pData   Pointer for the received data
    \param[in]      Len     Length of the received data
     
    \return         On success, zero is returned. On error, negative is returned    
    
    \sa             sl_FsClose sl_FsWrite sl_FsOpen        
    \note           belongs to \ref basic_api       
    \warning     
	\par            Example:
	\code	
	Status = sl_FsRead(FileHandle, 0, &readBuff[0], readSize);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsRead)
long sl_FsRead(long FileHdl, unsigned long Offset, unsigned char* pData, unsigned long Len);
#endif

/*!
    \brief write block of data to a file in storage device
    
    \param[in]      FileHdl  Pointer to the file (assigned from sl_FsOpen)  
    \param[in]      Offset   Offset to specific block to be written
    \param[in]      pData    Pointer the transmitted data to the storage device
    \param[in]      Len      Length of the transmitted data
     
    \return         On success, zero is returned. On error, negative is returned    
    
    \sa                     
    \note           belongs to \ref basic_api       
    \warning     
	\par            Example:
	\code	
	Status = sl_FsWrite(FileHandle, 0, &buff[0], readSize);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsWrite)
long sl_FsWrite(long FileHdl, unsigned long Offset, unsigned char* pData, unsigned long Len);
#endif

/*!
    \brief get info on a file
    
    \param[in]      pFileName    File's name
    \param[in]      Token        File's token
    \param[out]     pFsFileInfo Returns the File's Information: flags,file size, allocated size and Tokens 
     
    \return         On success, zero is returned. On error, negative is returned    
    
    \sa             sl_FsOpen        
    \note           belongs to \ref basic_api       
    \warning        
	\par            Example:
	\code	
	Status = sl_FsGetInfo("FileName.html",Token,&FsFileInfo);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsGetInfo)
int sl_FsGetInfo(unsigned char *pFileName,unsigned long Token,SlFsFileInfo_t* pFsFileInfo);
#endif

/*!
    \brief delete specific file from a storage or all files from a storage (format)
    
    \param[in]      pFileName    File's Name 
    \param[in]      Token        File's token     
    \return         On success, zero is returned. On error, negative is returned    
    
    \sa                     
    \note           belongs to \ref basic_api       
    \warning     
	\par            Example:
	\code	
	Status = sl_FsDel("FileName.html",Token);
	\endcode
*/
#if _SL_INCLUDE_FUNC(sl_FsDel)
int sl_FsDel(unsigned char *pFileName,unsigned long Token);
#endif
/*!

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif // __NVMEM_H__
