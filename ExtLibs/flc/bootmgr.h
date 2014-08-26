#ifndef __BOOTMGR__
#define __BOOTMGR__

/******************************************************************************

   If building with a C++ compiler, make all of the definitions in this header
   have a C binding.
  
*******************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
   Image file names
*******************************************************************************/
#define IMG_BOOT_INFO           "/sys/mcubootinfo.bin"
#define IMG_FACTORY_DEFAULT     "/sys/mcuimg1.bin"
#define IMG_USER_1              "/sys/mcuimg2.bin"
#define IMG_USER_2              "/sys/mcuimg3.bin"

/******************************************************************************
   Image status
*******************************************************************************/
#define IMG_STATUS_TESTING      0x12344321
#define IMG_STATUS_TESTREADY    0x56788765
#define IMG_STATUS_NOTEST       0xABCDDCBA

/******************************************************************************
   Active Image
*******************************************************************************/
#define IMG_ACT_FACTORY         0
#define IMG_ACT_USER1           1
#define IMG_ACT_USER2           2

/******************************************************************************
   Boot Info structure
*******************************************************************************/
typedef struct sBootInfo
{
  _u8  ucActiveImg;
  _u32 ulImgStatus;

}sBootInfo_t;


/******************************************************************************
  
   Mark the end of the C bindings section for C++ compilers.
  
*******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif /* __BOOTMGR__ */
