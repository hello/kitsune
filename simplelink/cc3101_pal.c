
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


#include <datatypes.h>
#include <simplelink.h>
#include <cc3101_pal.h>
#include <hw_types.h>
#include <pin.h>
#include <hw_memmap.h>
#include "hw_mcspi.h"
#include <hw_common_reg.h>
#include <rom.h>
#include <rom_map.h>
#include <spi.h>
#include <prcm.h>
#include <rom_map.h>
#include <hw_ints.h>
#include <interrupt.h>
#include <udma.h>
#include <udma_if.h>
#include <utils.h>

P_EVENT_HANDLER g_pHostIntHdl  = NULL;

#define REG_INT_MASK_SET 0x400F7088
#define REG_INT_MASK_CLR 0x400F708C

#define DMA_BUFF_SIZE_MIN 100

#define APPS_SOFT_RESET_REG             0x4402D000
#define OCP_SHARED_MAC_RESET_REG        0x4402E168
#define ARCM_SHSPI_RESET_REG            0x440250CC

#define SPI_IF_BIT_RATE                 20000000

volatile Fd_t g_SpiFd =0;

#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)
unsigned char g_ucDout[100];
unsigned char g_ucDin[100];
#include <osi.h>
OsiMsgQ_t DMAMsgQ;
char g_cDummy[4];
#endif

//******************************************************************************
// This defines the system Clock in Hz
//******************************************************************************

#define SYS_CLK 80000000
#define UNUSED(x) x=x

/*!
    \brief attempts to read up to len bytes from SPI channel into a buffer starting at pBuff.

	\param			pBuff		- 	points to first location to start writing the data

	\param			len			-	number of bytes to read from the SPI channel

	\return			upon successful completion, the function shall return Read Size.
					Otherwise, -1 shall be returned

    \sa             spi_Read_CPU , spi_Write_CPU
	\note
    \warning
*/
int spi_Read_CPU(unsigned char *pBuff, int len)
{
    unsigned long ulCnt;
    unsigned long ulStatusReg;
    unsigned long *ulDataIn;
    unsigned long ulTxReg;
    unsigned long ulRxReg;
	
    //Enable Chip Select
    //HWREG(0x44022128)&=~1;
    SPICSEnable(LSPI_BASE);

    //
    // Initialize local variable.
    //
    ulDataIn = (unsigned long *)pBuff;
    ulCnt = (len + 3) >> 2;
    ulStatusReg = LSPI_BASE+MCSPI_O_CH0STAT;
    ulTxReg = LSPI_BASE + MCSPI_O_TX0;
    ulRxReg = LSPI_BASE + MCSPI_O_RX0;

    //
    // Reading loop
    //
    while(ulCnt--)
    {
          while(!( HWREG(ulStatusReg)& MCSPI_CH0STAT_TXS ));
          HWREG(ulTxReg) = 0xFFFFFFFF;
          while(!( HWREG(ulStatusReg)& MCSPI_CH0STAT_RXS ));
          *ulDataIn = HWREG(ulRxReg);
          ulDataIn++;
    }

	//Disable  Chip Select
	//HWREG(0x44022128)|= 1;
    SPICSDisable(LSPI_BASE);
	
    return len;
}

/*!
    \brief attempts to write up to len bytes to the SPI channel

	\param			pBuff		- 	points to first location to start getting the data from

	\param			len			-	number of bytes to write to the SPI channel

	\return			upon successful completion, the function shall return write size.
					Otherwise, -1 shall be returned

    \sa             spi_Read_CPU , spi_Write_CPU
	\note			This function could be implemented as zero copy and return only upon successful completion
					of writing the whole buffer, but in cases that memory allocation is not too tight, the
					function could copy the data to internal buffer, return back and complete the write in
					parallel to other activities as long as the other SPI activities would be blocked untill
					the entire buffer write would be completed
    \warning
*/
int spi_Write_CPU(unsigned char *pBuff, int len)
{

  
    unsigned long ulCnt;
    unsigned long ulStatusReg;
    unsigned long *ulDataOut;
    unsigned long ulDataIn;
    unsigned long ulTxReg;
    unsigned long ulRxReg;

    //Enable Chip Select
    //HWREG(0x44022128)&=~1;
    SPICSEnable(LSPI_BASE);

    //
    // Initialize local variable.
    //
    ulDataOut = (unsigned long *)pBuff;
    ulCnt = (len +3 ) >> 2;
    ulStatusReg = LSPI_BASE+MCSPI_O_CH0STAT;
    ulTxReg = LSPI_BASE + MCSPI_O_TX0;
    ulRxReg = LSPI_BASE + MCSPI_O_RX0;

    //
    // Writing Loop
    //
    while(ulCnt--)
    {
          while(!( HWREG(ulStatusReg)& MCSPI_CH0STAT_TXS ));
          HWREG(ulTxReg) = *ulDataOut;
          while(!( HWREG(ulStatusReg)& MCSPI_CH0STAT_RXS ));
          ulDataIn = HWREG(ulRxReg);
          ulDataOut++;
    }

    //Disable  Chip Select
    //HWREG(0x44022128)|= 1;

    SPICSDisable(LSPI_BASE);
		
    UNUSED(ulDataIn);
    return len;

}


#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)
//*****************************************************************************
//
//! DMA SPI interrupt handler
//!
//! \param None
//!
//! This function
//!        1. Invoked when SPI Transaction Completes
//!
//! \return None.
//
//*****************************************************************************
void DmaSpiSwIntHandler()
{
    SPIIntClear(LSPI_BASE,SPI_INT_EOW);
    SPICSDisable(LSPI_BASE);
    osi_MsgQWrite(&DMAMsgQ,g_cDummy,OSI_NO_WAIT);

}

//*****************************************************************************
//
//! DMA error interrupt handler
//!
//! \param None
//!
//! This function
//!        1. Invoked when DMA operation is in error
//!
//! \return None.
//
//*****************************************************************************
void DmaErrIntHandler(void)
{
    HWREG(0x4402609c) = (3<<10);
    MAP_uDMAIntClear(MAP_uDMAIntStatus()); 
    
    //Should not reach here
    while(1)
    {
      
    }
    
}

void SetupDMAReceive(unsigned char *ucBuff,int len)
{

  SetupTransfer(UDMA_CH13_LSPI_TX,
                  UDMA_MODE_BASIC,
                  len/4,
                  UDMA_SIZE_32,
                  UDMA_ARB_1,
                  &g_ucDout[0],
                  UDMA_SRC_INC_NONE,
                  (void *)(LSPI_BASE+MCSPI_O_TX0),
                  UDMA_DST_INC_NONE); 

 SetupTransfer(UDMA_CH12_LSPI_RX,
                  UDMA_MODE_BASIC,
                  len/4,
                  UDMA_SIZE_32,
                  UDMA_ARB_1,
                  (void *)(LSPI_BASE+MCSPI_O_RX0),
                  UDMA_SRC_INC_NONE,
                  (void *)ucBuff,
                  UDMA_DST_INC_32);
 
 SPIWordCountSet(LSPI_BASE,len/4);

}

void SetupDMASend(unsigned char *ucBuff,int len)
{

  SetupTransfer(UDMA_CH13_LSPI_TX,
                  UDMA_MODE_BASIC,
                  len/4,
                  UDMA_SIZE_32,
                  UDMA_ARB_1,
                  (void *)ucBuff,
                  UDMA_SRC_INC_32,
                  (void *)(LSPI_BASE+MCSPI_O_TX0),
                  UDMA_DST_INC_NONE);


 SetupTransfer(UDMA_CH12_LSPI_RX,
                  UDMA_MODE_BASIC,
                  len/4,
                  UDMA_SIZE_32,
                  UDMA_ARB_1,
                  (void *)(LSPI_BASE+MCSPI_O_RX0),
                  UDMA_SRC_INC_NONE,
                  &g_ucDin[0],
                  UDMA_DST_INC_NONE);
 
 SPIWordCountSet(LSPI_BASE,len/4);


}

#endif


#define MAX_DMA_RECV_TRANSACTION_SIZE 4000
/*!
    \brief open spi communication port to be used for communicating with a SimpleLink device

	Given an interface name and option flags, this function opens the spi communication port
	and creates a file descriptor. This file descriptor can be used afterwards to read and
	write data from and to this specific spi channel.
	The SPI speed, clock polarity, clock phase, chip select and all other attributes are all
	set to hardcoded values in this function.

	\param	 		ifName		-	points to the interface name/path. The interface name is an
									optional attributes that the simple link driver receives
									on opening the device. in systems that the spi channel is
									not implemented as part of the os device drivers, this
									parameter could be NULL.
	\param			flags		-	option flags

	\return			upon successful completion, the function shall open the spi channel and return
					a non-negative integer representing the file descriptor.
					Otherwise, -1 shall be returned

    \sa             spi_Close , spi_Read , spi_Write
	\note
    \warning
*/
Fd_t spi_Open(char *ifName, unsigned long flags)
{
    unsigned long ulBase;

    //NWP master interface
    ulBase = LSPI_BASE;

    //Enable MCSPIA2
    PRCMPeripheralClkEnable(PRCM_LSPI,PRCM_RUN_MODE_CLK);

    //Disable Chip Select
    SPICSDisable(ulBase); 

    //Disable SPI Channel
    SPIDisable(ulBase);

    // Reset SPI
    SPIReset(ulBase);

    //
  // Configure SPI interface
  //
  SPIConfigSetExpClk(ulBase,PRCMPeripheralClockGet(PRCM_LSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_32));


		
#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)

    memset(g_ucDout,0xFF,100);
    //g_ucDout[0]=0xFF;
    UDMAInit();
    // Set DMA channel
    UDMAChannelSelect(UDMA_CH12_LSPI_RX, NULL);
    UDMAChannelSelect(UDMA_CH13_LSPI_TX, NULL);


    SPIFIFOEnable(ulBase,SPI_RX_FIFO);
    SPIFIFOEnable(ulBase,SPI_TX_FIFO);
    SPIDmaEnable(ulBase,SPI_RX_DMA);
    SPIDmaEnable(ulBase,SPI_TX_DMA);
    
    SPIFIFOLevelSet(ulBase,1,1);

    osi_InterruptRegister(INT_LSPI, (P_OSI_INTR_ENTRY)DmaSpiSwIntHandler);
    osi_InterruptRegister(UDMA_INT_ERR, (P_OSI_INTR_ENTRY)DmaErrIntHandler);
    SPIIntEnable(ulBase,SPI_INT_EOW);
    

    osi_MsgQCreate(&DMAMsgQ,"DMAQueue",sizeof(int),1);
   
#endif //CC3101_SPI_DMA, SL_PLATFORM_MULTI_THREADED   
    
    SPIEnable(ulBase);

    g_SpiFd = 1; 
    return g_SpiFd;
	
}
/*!
    \brief closes an opened spi communication port

	\param	 		fd			-	file descriptor of an opened SPI channel

	\return			upon successful completion, the function shall return 0.
					Otherwise, -1 shall be returned

    \sa             spi_Open
	\note
    \warning
*/
int spi_Close(Fd_t fd)
{
    unsigned long ulBase = LSPI_BASE;
   
    g_SpiFd = 0;

#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)
    UDMADeInit();
    SPIIntUnregister(ulBase);
    SPIFIFODisable(ulBase,SPI_RX_FIFO);
    SPIFIFODisable(ulBase,SPI_TX_FIFO);
    SPIDmaDisable(ulBase,SPI_RX_DMA);
    SPIDmaDisable(ulBase,SPI_TX_DMA);
#endif

    //Disable Chip Select
    SPICSDisable(LSPI_BASE);


    //Disable SPI Channel
    SPIDisable(ulBase);

    // Reset SPI
    SPIReset(ulBase);

    // Enable SPI Peripheral
    PRCMPeripheralClkDisable(PRCM_LSPI,PRCM_RUN_MODE_CLK);

    return 0;
}
int g_len=0;
int spi_Read(Fd_t fd, unsigned char *pBuff, int len)
{    
    int read_size = 0;
    if(fd!=1 || g_SpiFd!=1)
    return -1;

    g_len = len;
	if(len>DMA_BUFF_SIZE_MIN)
	{  

#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)
            char temp[4];
	    while (len>0)
	    {
	        if( len < MAX_DMA_RECV_TRANSACTION_SIZE)
	        {
	           SetupDMAReceive(&pBuff[read_size],len);
	           SPICSEnable(LSPI_BASE);
	           osi_MsgQRead(&DMAMsgQ,temp,OSI_WAIT_FOREVER);
	           read_size += len;
	           len = 0;
	        }
	        else
	        {
	            SetupDMAReceive(&pBuff[read_size],MAX_DMA_RECV_TRANSACTION_SIZE);
	            SPICSEnable(LSPI_BASE);
				osi_MsgQRead(&DMAMsgQ,temp,OSI_WAIT_FOREVER);
				read_size += MAX_DMA_RECV_TRANSACTION_SIZE;
	            len -= MAX_DMA_RECV_TRANSACTION_SIZE;
	        }
	    }
	#else //CC3101_SPI_DMA,SL_PLATFORM_MULTI_THREADED
		read_size += spi_Read_CPU(pBuff,len);
	#endif //CC3101_SPI_DMA,SL_PLATFORM_MULTI_THREADED
	}

	else
	{
		read_size += spi_Read_CPU(pBuff,len);
	}
    return read_size;
}

/*!
    \brief attempts to write up to len bytes to the SPI channel

	\param	 		fd			-	file descriptor of an opened SPI channel

	\param			pBuff		- 	points to first location to start getting the data from

	\param			len			-	number of bytes to write to the SPI channel

	\return			upon successful completion, the function shall return 0.
					Otherwise, -1 shall be returned

    \sa             spi_Open , spi_Read
	\note			This function could be implemented as zero copy and return only upon successful completion
					of writing the whole buffer, but in cases that memory allocation is not too tight, the
					function could copy the data to internal buffer, return back and complete the write in
					parallel to other activities as long as the other SPI activities would be blocked untill
					the entire buffer write would be completed
    \warning
*/
int spi_Write(Fd_t fd, unsigned char *pBuff, int len)
{
    int write_size = 0;

    if(fd!=1 || g_SpiFd!=1)
        return -1;

	if(len>DMA_BUFF_SIZE_MIN)
	{	
#if defined(CC3101_SPI_DMA) && defined(SL_PLATFORM_MULTI_THREADED)	

            char temp[4];
	    SetupDMASend(pBuff,len);    
	    SPICSEnable(LSPI_BASE);
	    osi_MsgQRead(&DMAMsgQ,temp,OSI_WAIT_FOREVER);
            write_size += len;
#else
	    write_size += spi_Write_CPU(pBuff,len);
            

#endif
	}

	else
	{
		write_size += spi_Write_CPU(pBuff,len);

	}
    return write_size;
}



void HostIntHanlder()
{
    if(g_pHostIntHdl != NULL)
    {
        g_pHostIntHdl();
    }
    else
    {
        while(1)
        {

        }
    }
}
/*!
    \brief register an interrupt handler for the host IRQ

	\param	 		InterruptHdl	-	pointer to interrupt handler function

	\param 			pValue			-	pointer to a memory strcuture that is passed to the interrupt handler.

	\return			upon successful registration, the function shall return 0.
					Otherwise, -1 shall be returned

    \sa
	\note			If there is already registered interrupt handler, the function should overwrite the old handler
					with the new one
    \warning
*/

int NwpRegisterInterruptHandler(P_EVENT_HANDLER InterruptHdl , void* pValue)    //do not know what to do with pValue
{

    if(InterruptHdl == NULL)
    {
        //De-register Interprocessor communication interrupt between App and NWP
        IntDisable(INT_NWPIC);
        IntUnregister(INT_NWPIC);
        IntPendClear(INT_NWPIC);
        g_pHostIntHdl = NULL;
    }
    else
    {
          g_pHostIntHdl = InterruptHdl;
          #ifdef SL_PLATFORM_MULTI_THREADED
             osi_InterruptRegister(INT_NWPIC, (P_OSI_INTR_ENTRY)HostIntHanlder);
          #else             
              IntRegister(INT_NWPIC, HostIntHanlder);
              IntEnable(INT_NWPIC);
          #endif
    }

  return 0;
}


/*!
    \brief 				Masks host IRQ

					
    \sa             		NwpUnMaskInterrupt

    \warning        
*/
void NwpMaskInterrupt()
{
	(*(unsigned long *)REG_INT_MASK_SET) = 0x1;
}


/*!
    \brief 				Unmasks host IRQ

					
    \sa             		NwpMaskInterrupt

    \warning        
*/
void NwpUnMaskInterrupt()
{
	(*(unsigned long *)REG_INT_MASK_CLR) = 0x1;
}


void NwpPowerOn(void)
{

    PinModeSet(PIN_11,PIN_MODE_1);
    PinModeSet(PIN_12,PIN_MODE_1);
    PinModeSet(PIN_13,PIN_MODE_1);
    PinModeSet(PIN_14,PIN_MODE_1);
    
    //Pinmux for Network Logs
    PinModeSet(PIN_60,PIN_MODE_3);
    PinModeSet(PIN_62,PIN_MODE_1);

    //SPI CLK GATING
    HWREG(0x440250C8) = 0;

    //WLAN PD ON
    HWREG(OCP_SHARED_MAC_RESET_REG) &= ~(0xC00);
    
    //Remove NWP Reset
    HWREG(APPS_SOFT_RESET_REG) &= ~4;

    //UnMask Host Interrupt
    NwpUnMaskInterrupt();   
    
    //NWP Wakeup
    PRCMNWPEnable();     

}

void NwpPowerOff(void)
{
    //Mask Host Interrupt
    NwpMaskInterrupt();
    
    //Switch to PFM Mode
    HWREG(0x4402F024) &= 0xF7FFFFFF;

    //Reset NWP
    HWREG(APPS_SOFT_RESET_REG) |= 4; 
    //WLAN PD OFF
    HWREG(OCP_SHARED_MAC_RESET_REG) |= 0xC00;

}
