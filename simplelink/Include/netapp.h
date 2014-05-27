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

#ifndef __NETAPP_H__
#define	__NETAPP_H__

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct
{
	unsigned long   PacketsSent;
	unsigned long   PacketsReceived;
	unsigned short  MinRoundTime;
	unsigned short  MaxRoundTime;
	unsigned short  AvgRoundTime;
    unsigned long   TestTime;
}SlPingReport_t;


typedef struct
{
    unsigned long  PingIntervalTime;       /* delay between pings, in miliseconds */
    unsigned short PingSize;               /* ping packet size in bytes           */
    unsigned short PingRequestTimeout;     /* timeout time for every ping in miliseconds  */
    unsigned long  TotalNumberOfAttempts;  /* max number of ping requests. 0 - forever    */
    unsigned long  Flags;                  /* flags                               */
    unsigned long  Ip;                     /* IPv4 address or IPv6 first 4 bytes  */
    unsigned long  Ip1OrPaadding;
    unsigned long  Ip2OrPaadding;
    unsigned long  Ip3OrPaadding;
}SlPingStartCommand_t;


/*  Http Server interface */

#define MAX_INPUT_STRING  		64 // because of WPA

#define MAX_AUTH_NAME_LEN		20
#define MAX_AUTH_PASSWORD_LEN	20
#define MAX_AUTH_REALM_LEN		20

#define MAX_DEVICE_URN_LEN (15+1)
#define MAX_DOMAIN_NAME_LEN	(24+1)

#define MAX_ACTION_LEN			30
#define MAX_TOKEN_NAME_LEN		20
#define MAX_TOKEN_VALUE_LEN		MAX_INPUT_STRING

/* Server Events */
#define SL_NETAPP_HTTPGETTOKENVALUE		1
#define SL_NETAPP_HTTPPOSTTOKENVALUE	2

/* Server Responses */
#define SL_NETAPP_RESPONSE_NONE			0
#define SL_NETAPP_HTTPSETTOKENVALUE		1

#define SL_NETAPP_FAMILY_MASK  0x80

typedef struct _slHttpServerString_t
{
	UINT8 len;
	UINT8 *data;
} slHttpServerString_t;

typedef struct _slHttpServerPostData_t
{
	slHttpServerString_t action;
    slHttpServerString_t  token_name;
	slHttpServerString_t token_value;
}slHttpServerPostData_t;

typedef union
{
  slHttpServerString_t  httpTokenName; /* SL_NETAPP_HTTPGETTOKENVALUE */
  slHttpServerPostData_t   httpPostData;  /* SL_NETAPP_HTTPPOSTTOKENVALUE */
} SlHttpServerEventData_u;

typedef union
{
  slHttpServerString_t token_value;
} SlHttpServerResponsedata_u;

typedef struct
{
   unsigned long            Event;
   SlHttpServerEventData_u	EventData;
}SlHttpServerEvent_t;

typedef struct
{
   unsigned long				Response;
   SlHttpServerResponsedata_u	ResponseData;
}SlHttpServerResponse_t;


typedef struct
{
    UINT32  lease_time;
    UINT32  ipv4_addr_start;
    UINT32  ipv4_addr_last;
}SlNetAppDhcpServerBasicOpt_t; 


/* NetApp application IDs */
#define SL_NET_APP_HTTP_SERVER_ID   (1)
#define SL_NET_APP_DHCP_SERVER_ID   (2)
#define SL_NET_APP_MDNS_ID          (4)
#define SL_NET_APP_DNS_SERVER_ID	(8)
#define SL_NET_APP_DEVICE_CONFIG_ID (16)

/* NetApp application set/get options */
#define NETAPP_SET_DHCP_SRV_BASIC_OPT        (0)

/* HTTP server set/get options */
#define NETAPP_SET_GET_HTTP_OPT_PORT_NUMBER    0
#define NETAPP_SET_GET_HTTP_OPT_AUTH_CHECK     1
#define NETAPP_SET_GET_HTTP_OPT_AUTH_NAME      2
#define NETAPP_SET_GET_HTTP_OPT_AUTH_PASSWORD  3
#define NETAPP_SET_GET_HTTP_OPT_AUTH_REALM     4

#define NETAPP_SET_MDNS_SET_CONT_QUERY_OPT    (1)
#define NETAPP_SET_MDNS_SET_SERVICE_ADD_OPT   (2)
#define NETAPP_SET_MDNS_SET_SERVICE_EXT_OPT   (3)

/* DNS server set/get options */
#define NETAPP_SET_GET_DNS_OPT_DOMAIN_NAME	0

/* Device Config set/get options */
#define NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN  0
#define NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME 1

typedef void (*P_SL_DEV_PING_CALLBACK)(SlPingReport_t*);

/*****************************************************************************

    API Prototypes

 *****************************************************************************/

/*!

    \addtogroup netapp
    @{

*/
/*!
    \brief Starts a network application

    Gets and starts network application for the current wlan mode

    \param[in] AppBitMap      application bitmap, could be one or combination of the following: \n
			                  - SL_NET_APP_HTTP_SERVER_ID (1)
			                  - SL_NET_APP_DHCP_SERVER_ID (2)
			                  - SL_NET_APP_MDNS_ID        (4)

    \return                   On error, negative number is returned

    \sa                       Stop one or more the above started applications using sl_NetAppStop
    \note                     This command activates the application for the current wlan mode (AP or STA)
    \warning
    \par                 Example:  
    \code                
    For example: Starting internal HTTP server + DHCP server: 
    sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID | SL_NET_APP_DHCP_SERVER_ID)
    
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_NetAppStart)
int sl_NetAppStart(UINT32 AppBitMap);
#endif
/*!
    \brief Stops a network application

    Gets and stops network application for the current wlan mode

    \param[in] AppBitMap    application id, could be one of the following: \n
                            - SL_NET_APP_HTTP_SERVER_ID (1)
                            - SL_NET_APP_DHCP_SERVER_ID (2)
                            - SL_NET_APP_MDNS_ID (4)

    \return                 On error, negative number is returned

    \sa
    \note                This command disables the application for the current active wlan mode (AP or STA)
    \warning
    \par                 Example:
    \code                
    
    For example: Stopping internal HTTP server: 
                         sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID); 
    
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_NetAppStop)
int sl_NetAppStop(UINT32 AppBitMap);
#endif


/*!
    \brief get host IP by name

    Obtain the IP Address of machine on network, by machine name.

    \param[in]  hostname        host name
    \param[in]  usNameLen       name length
    \param[out] out_ip_addr     This parameter is filled in with
                                host IP address. In case that host name is not
                                resolved, out_ip_addr is zero.
    \param[in]  family          protocol family

    \return                     On success, positive is returned.
                                On error, negative is returned
								POOL_IS_EMPTY may be return in case there are no resources in the system
					  			In this case try again later or increase MAX_CONCURRENT_ACTIONS

    \sa
    \note	Only one sl_NetAppDnsGetHostByName can be handled at a time.
			Calling this API while the same command is called from another thread, may result
			in one of the two scenarios:
			1. The command will wait (internal) until the previous command finish, and then be executed.
			2. There are not enough resources and POOL_IS_EMPTY error will return. 
			In this case, MAX_CONCURRENT_ACTIONS can be increased (result in memory increase) or try
			again later to issue the command.
    \warning
    \par  Example:
    \code
    unsigned long DestinationIP;
    sl_NetAppDnsGetHostByName("www.google.com", strlen("www.google.com"), &DestinationIP,SL_AF_INET);

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);
    Addr.sin_addr.s_addr = sl_Htonl(DestinationIP);
    AddrSize = sizeof(SlSockAddrIn_t);
    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByName)
int sl_NetAppDnsGetHostByName(char * hostname, unsigned short usNameLen, unsigned long* out_ip_addr,unsigned char family );
#endif


/*!
 		\brief Return service attributes like IP address, port and text according to service name
        \par
		The user sets a service name Full/Part (see example below), and should get:
		- IP of service
		- The port of service
		- The text of service

		Hence it can make a connection to the specific service and use it.
		It is similar to get host by name method.

		It is done by a single shot query with PTR type on the service name.

                  The command that is sent is from constant parameters and variables parameters.

	
        \param[in]     pService                   Service name can be full or partial. \n
															Example for full service name:
															1. PC1._ipp._tcp.local
                                                  2. PC2_server._ftp._tcp.local \n
                                                  .
                                                  Example for partial service name:
															1. _ipp._tcp.local
															2. _ftp._tcp.local

				   \param[in]    ServiceLen                  The length of the service name (in_pService).
				   \param[in]    Family				    IPv4 or IPv6 (SL_AF_INET , SL_AF_INET6).
        \param[out]    pAddr                      Contains the IP address of the service.
        \param[out]    pPort				      Contians the port of the service.
		\param[out]    pTextLen                   Has 2 options. One as Input field and the other one as output:
                                                  - Input: \n
                                                  Contains the max length of the text that the user wants to get.\n
                                                  It means that if the text len of service is bigger that its value than
															  the text is cut to inout_TextLen value.
                                                  - Output: \n
                                                   Contain the length of the text that is returned. Can be full text or part of the text (see above).

	    \param[out]   pOut_pText	  Contains the text of the service full or partial
															   
        \return       On success, zero is returned
			      	  POOL_IS_EMPTY may be return in case there are no resources in the system
					  In this case try again later or increase MAX_CONCURRENT_ACTIONS

        \note         The return's attributes belongs to the first service found.
		              There may be other services with the same service name that will response to the query.
                      The results of these responses are saved in the peer cache of the Device and should be read by another API.
					  
					  Only one sl_NetAppDnsGetHostByService can be handled at a time.
					  Calling this API while the same command is called from another thread, may result
 		   			  in one of the two scenarios:
					  1. The command will wait (internal) until the previous command finish, and then be executed.
					  2. There are not enough resources and POOL_IS_EMPTY error will return. 
					  In this case, MAX_CONCURRENT_ACTIONS can be increased (result in memory increase) or try
					  again later to issue the command.

        \warning      Text length can be 120 bytes only
*/
#if _SL_INCLUDE_FUNC(sl_NetAppDnsGetHostByService)
int sl_NetAppDnsGetHostByService(char				*pServiceName,	    // string containing all (or only part): name + subtype + service
						   unsigned char     ServiceLen,
						   unsigned char     Family,			// 4-IPv4 , 16-IPv6
						   unsigned long	*pAddr, 
						   unsigned short	*pPort,
						   unsigned long	*pTextLen,          // in: max len , out: actual len
						   char				*pText
						   );

#endif

/*!
		\brief Unregister mDNS service
        This function deletes the mDNS service from the mDNS package and the database.

        The mDNS service that is to be unregistered is a service that the application no longer wishes to provide. \n
        The service name should be the full service name according to RFC
        of the DNS-SD - meaning the value in name field in the SRV answer.
                    
        Examples for service names:
        1. PC1._ipp._tcp.local
        2. PC2_server._ftp._tcp.local

        \param[in]    pServiceName            Full service name. \n
                                                Example for service name:
                                                1. PC1._ipp._tcp.local
                                                2. PC2_server._ftp._tcp.local
        \param[in]    ServiceLen              The length of the service. 
		\return    On success, zero is returned 
		\sa		  sl_NetAppMDNSRegisterService

		\note        
		\warning 
        The size of the service length should be smaller than 255.


*/
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSUnRegisterService)
int sl_NetAppMDNSUnRegisterService(	const char		*pServiceName, 
									unsigned char   ServiceNameLen);


#endif

/*!
		\brief Register a new mDNS service
        \par
		This function registers a new mDNS service to the mDNS package and the DB.
                    
		This registered service is a service offered by the application.
        The service name should be full service name according to RFC
        of the DNS-SD - meaning the value in name field in the SRV answer.
		Example for service name:
        1. PC1._ipp._tcp.local
        2. PC2_server._ftp._tcp.local

        If the option is_unique is set, mDNS probes the service name to make sure
        it is unique before starting to announce the service on the network.
        Instance is the instance portion of the service name.

        \param[in]  ServiceLen         The length of the service.
        \param[in]  TextLen            The length of the service should be smaller than 64.
        \param[in]  port               The port on this target host port.
        \param[in]  TTL                The TTL of the service
        \param[in]  Options            bitwise parameters: \n
                                       - bit 0  - service is unique (means that the service needs to be unique)
                                       - bit 31  - for internal use if the service should be added or deleted (set means ADD).
                                       - bit 1-30 for future.

        \param[in]	pServiceName			  The service name.
                                       Example for service name: \n
                                                1. PC1._ipp._tcp.local
                                                2. PC2_server._ftp._tcp.local

        \param[in] pText                     The description of the service.
                                                should be as mentioned in the RFC
                                                (according to type of the service IPP,FTP...)

		\return     On success, zero is returned

        \sa			  sl_NetAppMDNSUnRegisterService

        \warning  	1) Temporary -  there is an allocation on stack of internal buffer.
                    Its size is NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH. \n
					It means that the sum of the text length and service name length cannot be bigger than
                    NETAPP_MDNS_MAX_SERVICE_NAME_AND_TEXT_LENGTH.\n
                    If it is - An error is returned. \n
                    2) According to now from certain constraints the variables parameters are set in the
					attribute part (contain constant parameters)
*/
#if _SL_INCLUDE_FUNC(sl_NetAppMDNSRegisterService)
int sl_NetAppMDNSRegisterService(	const char*		pServiceName, 
									unsigned char   ServiceNameLen,
									const char*		pText,
									unsigned char   TextLen,
									unsigned short  Port,
									unsigned long    TTL,
									unsigned long    Options);

#endif


/*!
    \brief send ICMP ECHO_REQUEST to network hosts

    Ping uses the ICMP protocol's mandatory ECHO_REQUEST


    \param[in]   pPingParams     Pointer to the ping request structure: \n
                                 - if flags parameter is set to 0, ping will report back once all requested pings are done (as defined by TotalNumberOfAttempts). \n
                                 - if flags parameter is set to 1, ping will report back after every ping, for TotalNumberOfAttempts.
                                 - if flags parameter is set to 2, ping will stop after the first successful ping, and report back for the successful ping, as well as any preceeding failed ones. 

    \param[in]   family          SL_AF_INET or  SL_AF_INET6
    \param[out]  pReport         Ping pReport
    \param[out]  pCallback       Callback function upon completion.
                                 If callback is NULL, the API is blocked until data arrives


    \return    On success, zero is returned. On error, -1 is returned
			   POOL_IS_EMPTY may be return in case there are no resources in the system
			   In this case try again later or increase MAX_CONCURRENT_ACTIONS

    \sa       sl_NetAppPingReport
    \note	  Only one sl_NetAppPingStart can be handled at a time.
			  Calling this API while the same command is called from another thread, may result
 		   	  in one of the two scenarios:
			  1. The command will wait (internal) until the previous command finish, and then be executed.
			  2. There are not enough resources and POOL_IS_EMPTY error will return. 
			  In this case, MAX_CONCURRENT_ACTIONS can be increased (result in memory increase) or try
			  again later to issue the command.
    \warning  
    \par      Example:
    \code     
    
    An example of sending 20 ping requests and reporting results to a callback routine when 
              all requests are sent:

              // callback routine
              void pingRes(SlPingReport_t* pReport)
              {
               // handle ping results 
              }
              
              // ping activation
              void PingTest()
              {
                 SlPingReport_t report;
                 SlPingStartCommand_t pingCommand;
	             
                 pingCommand.Ip = SL_IPV4_VAL(10,1,1,200);     // destination IP address is 10.1.1.200
                 pingCommand.PingSize = 150;                   // size of ping, in bytes 
                 pingCommand.PingIntervalTime = 100;           // delay between pings, in miliseconds
                 pingCommand.PingRequestTimeout = 1000;        // timeout for every ping in miliseconds
                 pingCommand.TotalNumberOfAttempts = 20;       // max number of ping requests. 0 - forever 
                 pingCommand.Flags = 0;                        // report only when finished
  
	             sl_PingStart( &pingCommand, SL_AF_INET, &report, pingRes ) )
	         }

    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_NetAppPingStart)
int sl_NetAppPingStart(SlPingStartCommand_t* pPingParams,unsigned char family,SlPingReport_t *pReport,const P_SL_DEV_PING_CALLBACK pPingCallback);
#endif

/*!
    \brief     Internal function for setting network application configurations

    \return    On success, zero is returned. On error, -1 is
               returned

    \param[in] AppId          Application id, could be one of the following: \n
                              - SL_NET_APP_HTTP_SERVER_ID
                              - SL_NET_APP_DHCP_SERVER_ID

    \param[in] SetOptions     set option, could be one of the following: \n
							NETAPP_SET_BASIC_OPT

    \param[in] OptionLen       option structure length

    \param[in] pOptionValues   pointer to the option structure
    \sa
    \note
    \warning
*/
#if _SL_INCLUDE_FUNC(sl_NetAppSet)
long sl_NetAppSet(unsigned char AppId ,unsigned char Option,unsigned char OptionLen, unsigned char *pOptionValue);
#endif

/*!
    \brief     Internal function for getting network applications configurations

    \return    On success, zero is returned. On error, -1 is
               returned

    \param[in] AppId          Application id, could be one of the following: \n
                              - SL_NET_APP_HTTP_SERVER_ID
                              - SL_NET_APP_DHCP_SERVER_ID

    \param[in] Options        Get option, could be one of the following: \n
							NETAPP_SET_BASIC_OPT

    \param[in] OptionLen     The length of the allocated memory as input, when the
                                        function complete, the value of this parameter would be
                                        the len that actually read from the device.
                                        If the device return length that is longer from the input
                                        value, the function will cut the end of the returned structure
                                        and will return 1

    \param[out] pValues      pointer to the option structure which will be filled with the response from the device

    \sa
    \note
    \warning
*/
#if _SL_INCLUDE_FUNC(sl_NetAppGet)
long sl_NetAppGet(unsigned char AppId, unsigned char Option,unsigned char *pOptionLen, unsigned char *pOptionValue);
#endif



/*!

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif	// __NETAPP_H__

