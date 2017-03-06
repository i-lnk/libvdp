// SearchDVS.h: interface for the CSearchDVS class.
//
//////////////////////////////////////////////////////////////////////

#ifdef PLATFORM_ANDROID
#include <linux/socket.h>
#include <linux/tcp.h>
#include <linux/in.h>
#endif

#include "libvdp.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_BIND_TIME 10

#define BROADCAST_SEND_PORT0			0x9888
#define BROADCAST_RECV_PORT0			0x8888

#define BROADCAST_SEND_PORT1			8600
#define BROADCAST_RECV_PORT1			6801

#define	SERVER_PACK_FLAG				0x03404324

typedef unsigned short 	WORD;
typedef unsigned char 	BYTE;
typedef int   			BOOL;
typedef unsigned int	DWORD;

typedef struct tagJBNV_SERVER_PACK{
	char			szIp[16];				//服务器Ip
	WORD			wMediaPort;				//流端口
	WORD			wWebPort;				//Http端口号
	WORD			wChannelCount;			//通道数量
	char			szServerName[32];		//服务器名
	DWORD			dwDeviceType;			//服务器类型
	DWORD			dwServerVersion;		//服务器版本
	WORD			wChannelStatic;			//通道状态(是否视频丢失)
	WORD			wSensorStatic;			//探头状态
	WORD			wAlarmOutStatic;		//报警输出状态
}JBNV_SERVER_PACK;

typedef struct tagJBNV_SERVER_PACK_EX{
	JBNV_SERVER_PACK jspack;
	BYTE	      	bMac[6]; 			//MAC地址
	BOOL	        bEnableDHCP;		//DHCP使能
	BOOL			bEnableDNS; 		//DNS使能
	DWORD			dwNetMask; 			//子网掩码
	DWORD			dwGateway; 			//网关
	DWORD			dwDNS;  			//DNS服务器
	DWORD			dwComputerIP;		//本地IP（暂时不用）
	BOOL	        bEnableCenter; 		//中心平台使能（暂时不用）
	DWORD			dwCenterIpAddress; 	//中心平台IP地址
	DWORD			dwCenterPort; 		//中心平台端口
	char			csServerNo[64]; 	//服务器序列号
	int				bEncodeAudio; 		//音频编码使能
}JBNV_SERVER_PACK_EX;

/*
typedef struct tagSEARCH_CMD{
	unsigned int dwFlag; 			//0x4844
	unsigned int dwCmd; 			//局域网搜索对应CMD_GET命令字
	
}SEARCH_CMD;
*/

//*********GK_Search***************************
typedef struct tagPHONE_INFO{
	char ssid[64];
	char psd[64];
}PHONE_INFO;

typedef struct tagSEARCH_CMD{
	unsigned int dwFlag;
	unsigned int dwCmd;
	PHONE_INFO search_info; 			 			
}SEARCH_CMD;


typedef struct tagJBNV_SERVER_MSG_DATA_EX{
	DWORD					dwSize;
	DWORD					dwPackFlag; // == SERVER_PACK_FLAG
	JBNV_SERVER_PACK_EX		jbServerPack;
}JBNV_SERVER_MSG_DATA_EX;

#pragma pack(push,1)

typedef struct _stBcastParam{
    char           szIpAddr[16];     //IP地址，可以修改
    char           szMask[16];       //子网掩码，可以修改
    char           szGateway[16];    //网关，可以修改
    char           szDns1[16];       //dns1，可以修改
    char           szDns2[16];       //dns2，可以修改
    char           szMacAddr[6];     //设备MAC地址
    unsigned short nPort;            //设备端口
    char           dwDeviceID[32];   //platform deviceid
    char           szDevName[80];    //设备名称
    char           sysver[16];       //固件版本
    char           appver[16];       //软件版本
    char           szUsrName[32];   //修改时会对用户认证
    char           szPassword[32];   //修改时会对用户认证
    char           sysmode;          //0->baby 1->HDIPCAM
    char           dhcp;             //0->禁止dhcp,1->充许DHCP
    char           other[2];         //other
    char           other1[20];       //other1
}BCASTPARAM, *PBCASTPARAM;

#pragma pack(pop)

/************************************************************************/
/*通讯格式：
	STARTCODE+CMD+BCASTPARAM
	定义：
	STARTCODE： short型，字符”HD”,0x4844
	CMD：short型，0x0101 表示获取
	0x0801 表示获取时的返回
	0x0102 表示设置
	0x0802 表示设置时的返回 */
/************************************************************************/

#define STARTCODE  0x4844  //HD
#define CMD_GET  0x0101
#define CMD_PHONE_SEARCH  0x0109
#define CMD_GET_RESPONSE  0x0801 
#define CMD_SET  0x0102
#define CMD_SET_RESPONSE  0x0802

#define MAX_SOCK 3

typedef struct{
	int 	sock_port;
	int 	sock_type;
	int 	broadcast;
}T_SOCK_ATTR,*PT_SOCK_ATTR;

class CSearchDVS  
{
public:
	CSearchDVS();
	virtual ~CSearchDVS();

public:
	char WifiName[64];
	char WifiPsd[64];
	int Open(char *ssid,char *psd);
	void Close();

	int SearchDVS();
private:	
	void OnMessageProc(const char *pszMessage, int iBufferSize, const char *pszIp);
	void ProcMessage(short nType, unsigned short nMessageLen, const char *pszMessage);
	void ProcMessageGoke(const char * lpMsg);
	void ProcMessageShenZhou(const char * lpMsg);
	void CloseSocket();
	
private:
	static void * RecvThread(void * param);
	void RecvProcess();

    static void * SendThread(void * param);
    void SendProcess();

private:
	int			socks[MAX_SOCK];
	T_SOCK_ATTR sockattrs[MAX_SOCK];
	
	bool		m_bRecvThreadRuning;
	pthread_t 	m_RecvThreadID;

    int m_bSendThreadRuning;
    pthread_t m_SendThreadID;

	JNIEnv * m_envLocal;
};
