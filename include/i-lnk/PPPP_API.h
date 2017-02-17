
#ifndef _PPPP_API_H_
#define _PPPP_API_H_


#if WIN32_DLL
	#define PPPP_API_EXPORTS 1
	#ifdef PPPP_API_EXPORTS
	#define PPPP_API_API __declspec(dllexport)
	#else
	#define PPPP_API_API __declspec(dllimport)
	#endif
#else
	#include <time.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <sys/param.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <netdb.h>
	#include <netinet/ip.h>
	#include <netinet/udp.h>
	#include <net/if.h>
	#include <net/if_arp.h>
	#include <arpa/inet.h>
	#include <stdio.h>
	#include <errno.h>
	#include <stdlib.h>
	#include <string.h>
	#include <fcntl.h>
	#include <assert.h>
	#include <stddef.h>
	#include <ctype.h>
	#include <unistd.h>
	#include <semaphore.h>
	#include <signal.h>
	#define PPPP_API_API
#endif //// #ifdef LINUX

#ifdef _ARC_COMPILER
#include "net_api.h"
#endif 

typedef enum
	{
	OPEN_INTERNET=0,
	RESTRICTED_CONE_NAT,
	RESTRICTED_PORT_CONE_NAT,
	SYMMETRIC_NAT,
	FULL_CONE_NAT,
	FIREWALL_BLOCK_UDP,
	SYMMETRIC_UDP_FIREWALL,
	BLOCKED,
	NAT_UNKNOWN
}t_nat_type;

typedef enum
	{
	MODE_P2P=0,
	MODE_RELAY,
	MODE_UNKNOWN
	}t_pppp_mode;

typedef enum {
	TERMINAL_TYPE_CLIENT=1,
	TERMINAL_TYPE_DEVICE,
	TERMINAL_TYPE_SUPERDEV,
	TERMINAL_TYPE_IPNODE,
	TERMINAL_TYPE_VGW,
	TERMINAL_TYPE_VGW_SERVER,
	TERMINAL_TYPE_UNKNOWN
}t_terminal_type;

typedef struct PPPP_Session
    {
    int  Skt; 				// Sockfd
    struct sockaddr_in RemoteAddr;	// Remote IP:Port
    struct sockaddr_in MyLocalAddr; // My Local IP:Port
    struct sockaddr_in MyWanAddr;	// My Wan IP:Port
    unsigned int ConnectTime; 			// Connection build in ? Sec Before
    char DID[24];					// Device ID
    char bCorD; 					// I am Client or Device, 0: Client, 1: Device
    char bMode; 					//session is p2p or relay 
    char Reserved[2];
    } st_PPPP_Session;

typedef struct PPPP_Session1
    {
    int  Skt; 				// Sockfd
    struct sockaddr_storage RemoteAddr;	// Remote IP:Port
    struct sockaddr_storage MyLocalAddr; // My Local IP:Port
    struct sockaddr_storage MyWanAddr;	// My Wan IP:Port
    unsigned int ConnectTime; 			// Connection build in ? Sec Before
    char DID[24];// Device ID
    char bCorD; 					// I am Client or Device, 0: Client, 1: Device
    char bMode;  
    char Reserved[2];
    } st_PPPP_Session1;


typedef struct lanSearchRet
{
char mIP[16];
char mDID[24];
}st_lanSearchRet;

typedef struct lanSearchExtRet
{
char mIP[16];
char mDID[24];
char mName[32];
}st_lanSearchExtRet;

typedef struct PPPP_NetInfo
{
	char bFlagInternet; 	// Internet Reachable? 1: YES, 0: NO
	char bFlagHostResolved; // P2P Server IP resolved? 1: YES, 0: NO
	char bFlagServerHello;	// P2P Server Hello? 1: YES, 0: NO
	char NAT_Type;			// NAT type, 0: Unknow, 1: IP-Restricted Cone type,   2: Port-Restricted Cone type, 3: Symmetric
	char MyLanIP[16];		// My LAN IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyLanIP will be "0.0.0.0"
	char MyWanIP[16];		// My Wan IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyWanIP will be "0.0.0.0"
} st_PPPP_NetInfo;

typedef struct PPPP_NetInfo1
{
	char bFlagInternet; 	// Internet Reachable? 1: YES, 0: NO
	char bFlagHostResolved; // P2P Server IP resolved? 1: YES, 0: NO
	char bFlagServerHello;	// P2P Server Hello? 1: YES, 0: NO
	char NAT_Type;			// NAT type, 0: Unknow, 1: IP-Restricted Cone type,   2: Port-Restricted Cone type, 3: Symmetric
	char MyLanIP[INET6_ADDRSTRLEN];		// My LAN IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyLanIP will be "0.0.0.0"
	char MyWanIP[INET6_ADDRSTRLEN];		// My Wan IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyWanIP will be "0.0.0.0"
	char MyLanIP6[INET6_ADDRSTRLEN];		// My LAN IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyLanIP will be "0.0.0.0"
	char MyWanIP6[INET6_ADDRSTRLEN];		// My Wan IP. If (bFlagInternet==0) || (bFlagHostResolved==0) || (bFlagServerHello==0), MyWanIP will be "0.0.0.0"
} st_PPPP_NetInfo1;


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    /*
    fun: get the lib version
  */
    PPPP_API_API unsigned int PPPP_GetAPIVersion();

	//作废
    PPPP_API_API int PPPP_QueryDID(
		const char* DeviceName,
		char* DID,
		int DIDBufSize
		);
	
    PPPP_API_API int PPPP_Initialize(
		char* server	//服务器加密字串
		);


     /*
     fun: initialize p2p lib enviorment, prepare all resourses
     in: 
        char* LicenseString license string encrypted which include info about how to accessing servers and locally verifying id
        UINT32 MaxNmbOfSession  max number of sessions can be setup, if <=0, will use default 8
        UINT32 MaxNmbOfChannel  max number of channels every session can support, if <=0, will use default 8
        UINT32 MaxSizeOfChannel max size of buffer for channel, if <=0, will use default 128K
        UINT32 MaxSizeOfPacket  max size of packet which can be sent or received, if <=0, will use default 1024
        char *NodeName          the name of this node <32bytes, if NULL, lib will use default name 'p2p_node'
     out:none
     ret:
         ERROR_PPPP_SUCCESSFUL
         ERROR_PPPP_ALREADY_INITIALIZED
         ERROR_PPPP_INVALID_PARAMETER
         ERROR_PPPP_INSUFFICIENT_RESOURC
         ERROR_PPPP_FAIL_TO_RESOLVE_NAME
   */
    PPPP_API_API int PPPP_InitializeExt(char* LicenseString,unsigned int MaxNmbOfSession,unsigned int MaxNmbOfChannel,unsigned int MaxSizeOfChannel,unsigned int MaxSizeOfPacket,char *NodeName);

    /*
    fun: deinitialze p2p lib, release all resourses 
    in: none
    out:none
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_NOT_INITIALIZED
   */     
	PPPP_API_API int PPPP_DeInitialize( void );

     /*
    fun: detects the conditions of network which this node resides 
    in: 
        UDP_Port udp port,as the following description
    out:
        st_PPPP_NetInfo* NetInfo network conditions
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_UDP_PORT_BIND_FAILED
        ERROR_PPPP_TIME_OUT
   */      
    PPPP_API_API int PPPP_NetworkDetect(
		st_PPPP_NetInfo1 *NetInfo, 
		unsigned short UDP_Port
		);

    /*
    fun: enable or disable the node to be superNode, if enable it, specify how many relays it can support
    in: 
        CHAR bOnOff    to be or not superNode, 0->disable, 1->enable
     out:
        none
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_NOT_INITIALIZED
  */         
    PPPP_API_API int PPPP_Share_Bandwidth(
		char bOnOff
		);

    /*
    fun: listen to accept connect
    in: 
        const CHAR* MyID            the p2p ID for this node
        const UINT32 TimeOut_Sec    timeout (second) the node keeps listening
        UINT16 UDP_Port             the UDP port which the node listens on
        CHAR bEnableInternet        enable or disable the node accept connection from internet
    out:
        none
    ret:
        >=0 the handle of connection setup
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_PREFIX,ERROR_PPPP_INVALID_ID
        ERROR_PPPP_NO_RELAY_SERVER_AVAILABLE
        ERROR_PPPP_MAX_SESSION
  */             
	PPPP_API_API int PPPP_Listen(
		const char* MyID, 
		const unsigned int TimeOut_Sec, 
		unsigned short UDP_Port, 
		char bEnableInternet,
		char **svrStr
		);

    PPPP_API_API int PPPP_Listen_Break( void );

    PPPP_API_API int PPPP_LoginStatus_Check( char* bLoginStatus );

    /*
    fun: connect to the target ID
    in: 
         const CHAR* TargetID       the target ID which connect to
         CHAR bEnableLanSearch      enable or disable search target in the same LAN
         UINT16 UDP_Port            the UDP port bind
         //UINT32 TimeOut_Sec         timeout to connect to
    out:
        none
    ret:
        >=0 the handle of connection setup
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_PREFIX,ERROR_PPPP_INVALID_ID
        ERROR_PPPP_NO_RELAY_SERVER_AVAILABLE
        ERROR_PPPP_MAX_SESSION
  */       
    PPPP_API_API int PPPP_Connect(
    	const char* TargetID, 
    	char bEnableLanSearch, 
    	unsigned short UDP_Port,
    	char **svrStr
    	);    

    /*
    fun: specify the p2p servers to connect to the target ID
    in: 
         const CHAR* TargetID       the target ID which connect to
         CHAR bEnableLanSearch      enable or disable search target in the same LAN
         UINT16 UDP_Port            the UDP port bind
         //UINT32 TimeOut_Sec         timeout to connect to
         //char *ServerString         the servers string encrypted 
    out:
        none
    ret:
        >=0 the handle of connection setup
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_FAIL_TO_RESOLVE_NAME
        ERROR_PPPP_INVALID_PREFIX,ERROR_PPPP_INVALID_ID
        ERROR_PPPP_NO_RELAY_SERVER_AVAILABLE
        ERROR_PPPP_MAX_SESSION
  */          
    PPPP_API_API int PPPP_ConnectVgw( const char* TargetID, char bEnableLanSearch, unsigned short UDP_Port );

    /*
    fun: specify the p2p servers to connect to the target ID
    in: 
         const CHAR* TargetID       the target ID which connect to
         CHAR bEnableLanSearch      enable or disable search target in the same LAN
         UINT16 UDP_Port            the UDP port bind
         //UINT32 TimeOut_Sec         timeout to connect to
         char *ServerString         the servers string encrypted 
    out:
        none
    ret:
        >=0 the handle of connection setup
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_FAIL_TO_RESOLVE_NAME
        ERROR_PPPP_INVALID_PREFIX,ERROR_PPPP_INVALID_ID
        ERROR_PPPP_NO_RELAY_SERVER_AVAILABLE
        ERROR_PPPP_MAX_SESSION
  */          
    PPPP_API_API int PPPP_ConnectByServer(
		const char *TargetID, 
		char bEnableLanSearch,  
		unsigned short UDP_Port, 
		char *ServerString
		);

    /*
    fun: specify the device IP to connect to the target ID
    in: 
         const CHAR* TargetID       the target ID which connect to
         const char *TargetIP       the known IP address of the device
         UINT16 UDP_Port            the UDP port bind
     out:
        none
    ret:
        >=0 the handle of connection setup
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_FAIL_TO_RESOLVE_NAME
        ERROR_PPPP_INVALID_PREFIX,ERROR_PPPP_INVALID_ID
        ERROR_PPPP_NO_RELAY_SERVER_AVAILABLE
        ERROR_PPPP_MAX_SESSION
  */          
    PPPP_API_API int PPPP_ConnectByIP(
		const char *TargetIP, 
		const char *TargetID
		);

	/*
	fun: break the session as the specified deviceID, maybe connecting or listening
	*/
    PPPP_API_API int PPPP_Connect_Break();

 	/*
	fun: check the status of  the session as the specified handler
	in: sessionhandle
	out:session info
	ret:
		=0 success
		<0 error, refer to PPPP_Error.h
	*/
    PPPP_API_API int PPPP_Check( int SessionHandle, st_PPPP_Session1* SInfo );

    /*
    fun: close the session which handle specified  or all sessions
    in: 
        INT32 SessionHandle     the session handle, if ==999 standfor all
        //CHAR CloseType          the type of session, force close or wait the close response of the peer
    out:
        none
    ret:
        =0 sucess
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_SESSION_HANDLE //if specify one
  */     
    PPPP_API_API int PPPP_Close(
		int SessionHandle
		);

	//same as the above now
    PPPP_API_API int PPPP_ForceClose( int SessionHandle );

     /*
    fun: wirte data to the specified channel of the specified session;
    in: 
         INT32 SessionHandle       the handle of the session you want to write data to
         UCHAR Channel             the channel of the session you want to write data into
         CHAR* DataBuf             data to write 
         INT32 DataSizeToWrite     data size to write
         //UINT32 Timeout_ms         timeout to write
    out:
        none
    ret:
        >=0 the bits of data have been writen successfully;
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_SESSION_HANDLE
        ERROR_PPPP_BUFFER_ERROR
        ERROR_PPPP_SESSION_CLOSED_REMOTE
        ERROR_PPPP_SESSION_CLOSED_TIMEOUT
        ERROR_PPPP_REMOTE_SITE_BUFFER_FULL
  */            
    PPPP_API_API int PPPP_Write(
		int SessionHandle, 
		unsigned char Channel, 
		char* DataBuf, 
		int DataSizeToWrite
		);
     /*
    fun: read data from the specified channel of the specified session;
    in: 
         INT32 SessionHandle       the handle of the session you want to read data
         UCHAR Channel             the channel of the session you want to read data from
         INT32 DataSizeToRead      data size to read
         UINT32 Timeout_ms         timeout to read
    out:
        CHAR* DataBuf             data readed
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_SESSION_HANDLE
        ERROR_PPPP_BUFFER_ERROR
        ERROR_PPPP_SESSION_CLOSED_REMOTE
        ERROR_PPPP_SESSION_CLOSED_TIMEOUT
        ERROR_PPPP_TIME_OUT
  */              
    PPPP_API_API int PPPP_Read(
		int SessionHandle,
		unsigned char Channel,
		char* DataBuf,
		int* DataSize,
		unsigned int TimeOut_ms
		);

     /*
    fun: check the bufer of the specified channel of the specified session;
    in: 
         INT32 SessionHandle       the handle of the session you want to check
         UCHAR Channel             the channel of the session you want to check buffer
    out:
        UINT32* WriteSize          the size of the write buffer used, buffer-writesize is the size which can be writen
        UINT32* ReadSize           the size of the read buffer used, can read
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_NOT_INITIALIZED
        ERROR_PPPP_INVALID_SESSION_HANDLE
        ERROR_PPPP_BUFFER_ERROR
        ERROR_PPPP_SESSION_CLOSED_REMOTE
        ERROR_PPPP_SESSION_CLOSED_TIMEOUT
  */            
    PPPP_API_API int PPPP_Check_Buffer( 
		int SessionHandle, 
		unsigned char Channel, 
		unsigned int* WriteSize, 
		unsigned int* ReadSize
		);

        /*
    fun: search p2p node in the same LAN
    in: search timeout <2000ms
    out:search result including node name, need give nodename in initiation
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_INVALID_PARAMETER //if timeout>2000 or SearchResult==NULL
        ERROR_PPPP_UDP_PORT_BIND_FAILED

    PPPP_API_API int PPPP_Search(
		unsigned int TimeOut,
		st_lanSearchRet *SearchResult
		);

   
    fun: search p2p node in the same LAN
    in: search timeout <2000ms
    out:search result
    ret:
        ERROR_PPPP_SUCCESSFUL
        ERROR_PPPP_INVALID_PARAMETER //if timeout>2000 or SearchResult==NULL
        ERROR_PPPP_UDP_PORT_BIND_FAILED
   */
    PPPP_API_API int PPPP_SearchExt(
		st_lanSearchExtRet * SearchExtResult, 
		unsigned int MaxNmbOfNode,
		unsigned int TimeOut_Ms
		);

	/*
	fun: break the session as the specified deviceID, maybe connecting or listening
	*/
    PPPP_API_API int PPPP_Break(char *szDID);
#ifdef __cplusplus
}
#endif // __cplusplus
#endif ////#ifndef _PPPP_API___INC_H

