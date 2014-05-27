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

#ifndef __NETCFG_H__
#define	__NETCFG_H__

#ifdef	__cplusplus
extern "C" {
#endif


/*****************************************************************************

    API Prototypes

 *****************************************************************************/

/*!

    \addtogroup netcfg
    @{

*/

typedef enum
{
	SL_MAC_ADDRESS_SET          = 1,
	SL_MAC_ADDRESS_GET          = 2,          
	SL_IPV4_STA_GET_INFO        = 3,
	SL_IPV4_STA_DHCP_ENABLE     = 4,
	SL_IPV4_STA_STATIC_ENABLE   = 5,
    SL_IPV4_AP_GET_INFO         = 6,
    SL_IPV4_AP_STATIC_ENABLE    = 7,
    MAX_SETTINGS = 0xFF
}Sl_NetCfg_e;


typedef struct
{
    unsigned long  ipV4;
    unsigned long  ipV4Mask;
    unsigned long  ipV4Gateway;
    unsigned long  ipV4DnsServer;
}_NetCfgIpV4Args_t;


#define SL_MAC_ADDR_LEN      6


#define SL_IPV4_VAL(add_3,add_2,add_1,add_0)     ( (((unsigned long)add_3 << 24) & 0xFF000000) | (((unsigned long)add_2 << 16) & 0xFF0000) | (((unsigned long)add_1 << 8) & 0xFF00) | ((unsigned long)add_0 & 0xFF) )
#define SL_IPV4_BYTE(val,index)                   ( (val >> (index*8)) & 0xFF )

/*!
    \brief      Set MAC address to the Device 
                The MAC address will be saved in the NVMEM - If using NVMEM this should be done once

    \return    On success, zero is returned. On error, -1 is 
               returned
     
     
    \param[in]  newMacAddress      -   Accepts 6 bytes buffer with the new MAC address
                                       
    \sa         sl_NetCfgSet        

    \note		Seeting a MAC address is optional - If not set the HW will asign a unique address

    \warning  

    \par        Example:
    \code 

	//Set MAC address: 08:00:28:22:69:31 
	SL_MAC_ADDR_SET("\x8\x0\x28\x22\x69\x31");

	// Or 
	unsigned char MAC_Address[6];
	MAC_Address[0] = 0x8;
	MAC_Address[1] = 0x0;
	MAC_Address[2] = 0x28;
	MAC_Address[3] = 0x22;
	MAC_Address[4] = 0x69;
	MAC_Address[5] = 0x31;
	SL_MAC_ADDR_SET(MAC_Address);

    \endcode 

*/
#define SL_MAC_ADDR_SET(newMacAddress)        sl_NetCfgSet(SL_MAC_ADDRESS_SET,1,SL_MAC_ADDR_LEN,(unsigned char *)newMacAddress)
/*!
    \brief      Get the device MAC address
                The returned MAC address is from the NVMEM,if not exist the MAC address will be taken from the Hardware 

    \return    On success, zero is returned. On error, -1 is 
               returned
     
     
    \param[out]  newMacAddress      -  Fills 6 bytes of MAC address
                                       
    \sa         sl_NetCfgSet

    \note 

    \warning 
    
    \par        Example:
    \code 

                unsigned char MAC_Address[6];
                SL_MAC_ADDR_GET(MAC_Address);

    \endcode
*/
#define SL_MAC_ADDR_GET(currentMacAddress)    { \
	                                                unsigned char macAddressLen = SL_MAC_ADDR_LEN; \
	                                                sl_NetCfgGet(SL_MAC_ADDRESS_GET,NULL,&macAddressLen,(unsigned char *)currentMacAddress); \
	                                             }

/*!
    \brief      Setting a static IP address to the device working in STA mode
				The IP address will be stored in the Serial Flash, this should be done once if Serial Flash is been used
				In order to disable the static IP and get the address assigned from DHCP one should use SL_STA_IPV4_DHCP_SET
   
   
    \param[in] ip - unsigned long IP address 

    \param[in] mask - unsigned long Subnet mask for this station

    \param[in] gateway - unsigned long Default gateway address

    \param[in] dns - unsigned long DNS server address

    \sa         sl_NetCfgSet
	            SL_STA_IPV4_DHCP_SET
				SL_IPV4_VAL

    \note 

    \warning

    \par        Example:
    \code 

     SL_STA_IPV4_ADDR_SET( SL_IPV4_VAL(10,1,1,201),    
		                        SL_IPV4_VAL(255,255,255,0), 
		                        SL_IPV4_VAL(10,1,1,1),      
		                        SL_IPV4_VAL(8,16,32,64)); 

    \endcode
*/
#define SL_STA_IPV4_ADDR_SET(ip,mask,gateway,dns)    {  \
                                                              _NetCfgIpV4Args_t ipV4;              \
														      ipV4.ipV4=(unsigned long)ip;                                                             \
														      ipV4.ipV4Mask= (unsigned long)mask;                                                      \
														      ipV4.ipV4Gateway= (unsigned long)gateway;                                                \
														      ipV4.ipV4DnsServer= (unsigned long)dns;                                                  \
														      sl_NetCfgSet(SL_IPV4_STA_STATIC_ENABLE,1,sizeof(_NetCfgIpV4Args_t),(unsigned char *)&ipV4); \
                                                      }

/*!
    \brief      Setting a static IP address to the device working in AP mode
				The IP address will be stored in the Serial Flash, this should be done once if Serial Flash is been used
   
    \param[in] ip - unsigned long IP address 

    \param[in] mask - unsigned long Subnet mask for this station

    \param[in] gateway - unsigned long Default gateway address

    \param[in] dns - unsigned long DNS server address

    \sa         sl_NetCfgSet
				SL_IPV4_VAL

    \note 

    \warning

    \par        Example:
    \code 

     SL_AP_IPV4_ADDR_SET( SL_IPV4_VAL(10,1,1,201),    
		                        SL_IPV4_VAL(255,255,255,0), 
		                        SL_IPV4_VAL(10,1,1,1),      
		                        SL_IPV4_VAL(8,16,32,64)); 

    \endcode
*/
#define SL_AP_IPV4_ADDR_SET(ip,mask,gateway,dns)    {  \
                                                              _NetCfgIpV4Args_t ipV4;              \
														      ipV4.ipV4=(unsigned long)ip;                                                             \
														      ipV4.ipV4Mask= (unsigned long)mask;                                                      \
														      ipV4.ipV4Gateway= (unsigned long)gateway;                                                \
														      ipV4.ipV4DnsServer= (unsigned long)dns;                                                  \
														      sl_NetCfgSet(SL_IPV4_AP_STATIC_ENABLE,1,sizeof(_NetCfgIpV4Args_t),(unsigned char *)&ipV4); \
                                                      }




/*!
    \brief      
	Set IP address by DHCP to Serial Flash using WLAN station mode.
                This should be done once if using Serial Flash.
                This is the system's default mode for acquiring an IP address after WLAN connection.

    \return    On success, zero is returned. On error, -1 is 
               returned
   
    \param

    \sa        sl_NetCfgSet 

    \note 

    \warning

    \par        Example:
    \code
                SL_STA_IPV4_DHCP_SET();
    \endcode

*/
#define SL_STA_IPV4_DHCP_SET()                            sl_NetCfgSet(SL_IPV4_STA_DHCP_ENABLE,1,0,NULL)

/*!
    \brief     Get static IP address from Serial Flash for WLAN station mode.  
   
    \param[out] ip - IP address for this station

    \param[out] mask - Subnet mask for this station

    \param[out] gateway - Default gateway address

    \param[out] dns - DNS server address

    \param[out] isDhcp - True if IP address was set by DHCP, False if used static IP address

    \sa         

    \note 

    \warning   

    \par        Example:
    \code       

	unsigned long ip_add =0;                                                                     
	unsigned long mask =0;                                                                       
	unsigned long gw = 0;                                                                        
	unsigned long dns =0;                                                                        
	unsigned char isDhcp =0;                                                                     
	SL_STA_IPV4_ADDR_GET(&ip_add,&mask,&gw,&dns,&isDhcp);                                          
	printf("DHCP is %s IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d DNS %d.%d.%d.%d\n",        
			(isDhcp > 0) ? "ON" : "OFF",                                                           
			SL_IPV4_BYTE(ip_add,3),SL_IPV4_BYTE(ip_add,2),SL_IPV4_BYTE(ip_add,1),SL_IPV4_BYTE(ip_add,0), 
			SL_IPV4_BYTE(mask,3),SL_IPV4_BYTE(mask,2),SL_IPV4_BYTE(mask,1),SL_IPV4_BYTE(mask,0),         
			SL_IPV4_BYTE(gw,3),SL_IPV4_BYTE(gw,2),SL_IPV4_BYTE(gw,1),SL_IPV4_BYTE(gw,0),                 
			SL_IPV4_BYTE(dns,3),SL_IPV4_BYTE(dns,2),SL_IPV4_BYTE(dns,1),SL_IPV4_BYTE(dns,0));     

    \endcode

*/
#define SL_STA_IPV4_ADDR_GET(ip,mask,gateway,dns,isDHCP)    {  \
	                                                             unsigned char len = sizeof(_NetCfgIpV4Args_t);                       \
																 unsigned char dhcpIsOn;                                              \
                                                                 _NetCfgIpV4Args_t ipV4;                                               \
														         sl_NetCfgGet(SL_IPV4_STA_GET_INFO,&dhcpIsOn,&len,(unsigned char *)&ipV4);     \
																 *ip=ipV4.ipV4;                                                       \
														         *mask=ipV4.ipV4Mask;                                                 \
														         *gateway=ipV4.ipV4Gateway;                                           \
														         *dns=ipV4.ipV4DnsServer;                                             \
																 *isDHCP = dhcpIsOn;                                                  \
	                                                          }     


/*!
    \brief     Get static IP address from Serial Flash for WLAN AP mode.  
   
    \param[out] ip - IP address for this station

    \param[out] mask - Subnet mask for this station

    \param[out] gateway - Default gateway address

    \param[out] dns - DNS server address

    \param[out] isDhcp - True if IP address was set by DHCP, False if used static IP address

    \sa         

    \note 

    \warning   

    \par        Example:
    \code       

	unsigned long ip_add =0;                                                                     
	unsigned long mask =0;                                                                       
	unsigned long gw = 0;                                                                        
	unsigned long dns =0;                                                                        
	                                                                 
	SL_AP_IPV4_ADDR_GET(&ip_add,&mask,&gw,&dns);                                          
	printf("IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d DNS %d.%d.%d.%d\n",                                                     
			SL_IPV4_BYTE(ip_add,3),SL_IPV4_BYTE(ip_add,2),SL_IPV4_BYTE(ip_add,1),SL_IPV4_BYTE(ip_add,0), 
			SL_IPV4_BYTE(mask,3),SL_IPV4_BYTE(mask,2),SL_IPV4_BYTE(mask,1),SL_IPV4_BYTE(mask,0),         
			SL_IPV4_BYTE(gw,3),SL_IPV4_BYTE(gw,2),SL_IPV4_BYTE(gw,1),SL_IPV4_BYTE(gw,0),                 
			SL_IPV4_BYTE(dns,3),SL_IPV4_BYTE(dns,2),SL_IPV4_BYTE(dns,1),SL_IPV4_BYTE(dns,0));     

    \endcode

*/
#define SL_AP_IPV4_ADDR_GET(ip,mask,gateway,dns)             {  \
	                                                             unsigned char len = sizeof(_NetCfgIpV4Args_t);                       \
																 unsigned char dhcpIsOn;                                              \
                                                                 _NetCfgIpV4Args_t ipV4;                                               \
														         sl_NetCfgGet(SL_IPV4_AP_GET_INFO,&dhcpIsOn,&len,(unsigned char *)&ipV4);     \
																 *ip=ipV4.ipV4;                                                       \
														         *mask=ipV4.ipV4Mask;                                                 \
														         *gateway=ipV4.ipV4Gateway;                                           \
														         *dns=ipV4.ipV4DnsServer;                                             \
	                                                          }     


/*!
    \brief     Internal function for setting network configurations

    \return    On success, zero is returned. On error, -1 is 
               returned
   
    \param[in] ConfigId   configuration id
    \param[in] ConfigOpt  configurations option
    \param[in] ConfigLen  configurations len
    \param[in] pValues    configurations values

    \sa         
    \note 
    \warning     
*/


#if _SL_INCLUDE_FUNC(sl_NetCfgSet)
long sl_NetCfgSet(unsigned char ConfigId ,unsigned char ConfigOpt, unsigned char ConfigLen, unsigned char *pValues);
#endif




/*!
    \brief     Internal function for getting network configurations

    \return    On success, zero is returned. On error, -1 is 
               returned
   
    \param[in] ConfigId      configuration id

    \param[out] pConfigOpt   Get configurations option 

    \param[out] pConfigLen   The length of the allocated memory as input, when the
                                        function complete, the value of this parameter would be
                             the len that actually read from the device.\n 
                                        If the device return length that is longer from the input 
                                        value, the function will cut the end of the returned structure
                                        and will return 1

    \param[out] pValues - get configurations values

    \sa         

    \note 

    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_NetCfgGet)
long sl_NetCfgGet(unsigned char ConfigId ,unsigned char *pConfigOpt, unsigned char *pConfigLen, unsigned char *pValues);
#endif

/*!

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif	// __NETAPP_H__

