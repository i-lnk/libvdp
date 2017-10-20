// SearchDVS.cpp: implementation of the CSearchDVS class.
//
//////////////////////////////////////////////////////////////////////

#include "utility.h"
#include "SearchDVS.h"

extern COMMO_LOCK		g_CallbackContextLock;

#ifdef PLATFORM_ANDROID
#include <jni.h>

extern JavaVM *         g_JavaVM ;
#endif

extern jmethodID 		g_CallBack_SearchResults;
extern jobject 			g_CallBack_Handle;

static CSearchDVS *     pSearchDVS = NULL;

CSearchDVS::CSearchDVS()
{
    m_bRecvThreadRuning = false;	
    m_RecvThreadID = 0 ;
    m_envLocal = NULL;

    m_bSendThreadRuning = 0;
    m_SendThreadID = 0;

	for(int i = 0;i < MAX_SOCK;i++){
		socks[i] = -1;
	}

	T_SOCK_ATTR sets[MAX_SOCK] = {
		{BROADCAST_RECV_PORT0,SOCK_DGRAM,1},	// for broadcast by goke
		{BROADCAST_RECV_PORT1,SOCK_DGRAM,1},	// for broadcast by goke
		{1025,SOCK_DGRAM,1},					// for cooee udp connection by ov788
	};

	memcpy(sockattrs,sets,sizeof(sets));

    pSearchDVS = this;
}

CSearchDVS::~CSearchDVS(){
	Close();
}

void CSearchDVS::CloseSocket()
{
	for(int i = 0;i < MAX_SOCK;i++){
		if(socks[i] < 0) continue;
		close(socks[i]);
//        shutdown(socks[i], SHUT_RDWR);
		socks[i] = -1;
	}
}

//创建socket(多播/广播)
int CSearchDVS::Open(char *ssid,char *psd)
{    

	Log3("Get Wifiname is :%s",ssid);
	Log3("Get wifipwd is %s",psd);

	strcpy(WifiName,ssid);
	strcpy(WifiPsd,psd);
	
	int	r;

	int i = 0;

	for(i = 0;i < MAX_SOCK;i++){
		socks[i] = socket(AF_INET, sockattrs[i].sock_type, 0);
		if (socks[i] < 0){
			Log3("open socket for search failed.\n");
			goto jumperr;
		}

		if(sockattrs[i].sock_type == SOCK_DGRAM){
			if(sockattrs[i].broadcast){
				int broadcast = 1;
				
				r = setsockopt(socks[i], SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(int));
				if (r < 0){
					Log3("open socket for broadcast failed.\n");
					goto jumperr;
				}
			}
		}

		int f = 1;

		if(setsockopt(socks[i], SOL_SOCKET, SO_REUSEADDR, &f, sizeof(f)) < 0){   
      		Log3("set socket reuseful failed.\n");
      		goto jumperr;
   		}   
	    
	    struct sockaddr_in servaddr;
		memset((char *)&servaddr, 0, sizeof(servaddr));  

		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(sockattrs[i].sock_port);
		
    	r = bind(socks[i], (sockaddr *)&servaddr, sizeof(servaddr) );
    	if (r < 0){
			Log3("set socket port:[%d] for:[%d] failed with error:[%d].\n",sockattrs[i].sock_port,socks[i],errno);
    		close(socks[i]);
			socks[i] = -1;
			continue;
    	}

		if(sockattrs[i].sock_type == SOCK_STREAM){
			listen(socks[i],8);
		}

		Log3("==========>>>> open device find socket:[%d] port:[%d].\n",socks[i],sockattrs[i].sock_port);
	}
	
    m_bSendThreadRuning = 1;
    pthread_create(&m_SendThreadID, NULL, CSearchDVS::SendThread, this);

    m_bRecvThreadRuning = 1;
    pthread_create(&m_RecvThreadID, NULL, CSearchDVS::RecvThread, this);
    
	return 1;

jumperr:

	for(i = 0;i < MAX_SOCK;i++){
		if(socks[i] < 0) continue;
		close(socks[i]);
		socks[i] = -1;
	}

	return 0;
}

//关闭socket(广播)
void CSearchDVS::Close()
{
	m_bRecvThreadRuning = 0;
	m_bSendThreadRuning = 0;
	
    CloseSocket();

    if (m_SendThreadID!= 0) 
	{
        pthread_join(m_SendThreadID, NULL);
        m_SendThreadID = 0;
	}
    
	if (m_RecvThreadID != 0) 
	{
        pthread_join(m_RecvThreadID, NULL);
        m_RecvThreadID = 0;
	}
}

int CSearchDVS::SearchDVS()
{  
	struct sockaddr_in servAddr;

	PHONE_INFO phone_info;
	memset(&phone_info,0,sizeof(phone_info));
	strcpy(phone_info.ssid,WifiName);
	strcpy(phone_info.psd,WifiPsd);
	
	Log3("Check WIFI's Name Is:%s",phone_info.ssid);
	SEARCH_CMD	searchCmd = {STARTCODE,CMD_PHONE_SEARCH,phone_info};

	memset(&servAddr, 0, sizeof(struct sockaddr_in));

	ssize_t nbytesSend = -1;
	
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	servAddr.sin_port = htons(BROADCAST_SEND_PORT0);
    
	nbytesSend = sendto(socks[0], &searchCmd, sizeof(searchCmd), 0, (sockaddr *)&servAddr, sizeof(struct sockaddr_in));

	Log3("broadcast search msg to sock:[%d] port:[%d][%d] ret = %ld,err = %d.",
		socks[0],ntohs(servAddr.sin_port),BROADCAST_SEND_PORT0,nbytesSend,errno);
    
    servAddr.sin_port = htons(BROADCAST_SEND_PORT1);
	char * p = (char*)&searchCmd;
    *((short*)p) = (short)STARTCODE;
    p += sizeof(short);
    *((short*)p) = (short)CMD_GET;

	nbytesSend = sendto(socks[1], &searchCmd, sizeof(short) * 2, 0, (sockaddr *)&servAddr, sizeof(struct sockaddr_in));

	Log3("broadcast search msg to sock:[%d] port:[%d][%d] ret = %ld,err = %d.",
		socks[1],ntohs(servAddr.sin_port),BROADCAST_SEND_PORT1,nbytesSend,errno);
    
	return 1;
}

void *CSearchDVS::SendThread(void * param)
{
    CSearchDVS *pSearchDVS = (CSearchDVS*)param;
    pSearchDVS->SendProcess();
    return NULL;
}

//接收dvs发送来的信息
void* CSearchDVS::RecvThread(void *pParam)
{
    CSearchDVS* pSearchDVS = (CSearchDVS*)pParam ;
 
#ifdef PLATFORM_ANDROID
    bool isAttached = false;
    int status;
    
    status = g_JavaVM->GetEnv((void **) &(pSearchDVS->m_envLocal), JNI_VERSION_1_4); 
    if(status < 0) 
    { 
        status = g_JavaVM->AttachCurrentThread(&(pSearchDVS->m_envLocal), NULL); 
        if(status < 0)
        {
            Log("AttachCurrentThread Failed!!");
            return NULL;
        }
        isAttached = true; 
    }
#endif
    
    pSearchDVS->RecvProcess() ;

#ifdef PLATFORM_ANDROID
    if(isAttached) 
        g_JavaVM->DetachCurrentThread();
#endif

    return NULL;
    
}

void CSearchDVS::SendProcess()
{ 
    int i;
    for(i = 0; i < 30; i++){
        SearchDVS();
		if(m_bSendThreadRuning != 1){
			Log3("broadcast search msg send proc stop now.");
			break;
		}
        sleep(1);
    }    
}

void CSearchDVS::RecvProcess()
{
    //F_LOG;
    
	struct timeval to = {0,500};
	struct sockaddr_in caddr;

	fd_set fds;

	int maxsock = -1;
	int i = 0;
	int err = 0;

	char tmps[1500] = {0};
	char * ipstr = NULL;

	Log3("client start device search recv process.\n");
    
	while (m_bRecvThreadRuning)
	{  
		FD_ZERO(&fds);

		for(i = 0;i < MAX_SOCK;i++){
			if(socks[i] > 0){
				FD_SET(socks[i],&fds);
			}
			if(socks[i] > maxsock){
				maxsock = socks[i];
			}
		}

		err = select(maxsock+1,&fds,NULL,NULL,&to);

		if(err < 0){
			Log3("select search socket failed:[%d] on socket.",errno);
//			goto jumperr;
		}else if(err == 0){
//          Log3("select timeout");
			continue;
		}else{
			// for udp broadcast message listen.
			for(i=0;i<MAX_SOCK;i++){
				if(FD_ISSET(socks[i],&fds)){
					int alens = sizeof(caddr);
					
					int bytes = recvfrom(
						socks[i],tmps,sizeof(tmps),0,
						(struct sockaddr *)&caddr,
						(socklen_t *)&alens
						);

					if(bytes < 0){
						Log3("recv from socket:[%d] failed:[%d].",socks[i],errno);
//						goto jumperr;
						continue;
					}else if(bytes == 0){
						continue;
					}else{
                        Log3("recv from socket:[%d] with lens:[%d].",socks[i],bytes);
						ipstr = (char*)inet_ntoa(caddr.sin_addr);
						OnMessageProc(tmps,bytes,(const char*)ipstr);
					}
				}
			}
        }
	}

jumperr:

	Log3("client close device search recv process.\n");

	return;
    
}

void CSearchDVS::ProcMessageShenZhou(const char * lpMsg){
	char szTemp[128] = {0};

    if(g_CallBack_Handle == NULL && g_CallBack_SearchResults == NULL){
		return;
    }
		
	jstring jdevname = NULL;
	jstring jipaddr = NULL;
	jstring jdeviceid = NULL;
    jstring jmac = NULL;
    
	int nPort=0;
	int sysmode=0;

	PBCASTPARAM hMsg = (PBCASTPARAM)lpMsg;

	memset(szTemp, 0, sizeof(szTemp));
	sprintf(szTemp, "%x:%x:%x:%x:%x:%x", hMsg->szMacAddr[0],
	    hMsg->szMacAddr[1],
	    hMsg->szMacAddr[2],
	    hMsg->szMacAddr[3],
	    hMsg->szMacAddr[4],
	    hMsg->szMacAddr[5]
	    );

	jdevname = m_envLocal->NewStringUTF(hMsg->szDevName);
	jipaddr = m_envLocal->NewStringUTF(hMsg->szIpAddr);
	jdeviceid = m_envLocal->NewStringUTF(hMsg->dwDeviceID);
    jmac = m_envLocal->NewStringUTF(szTemp);

	nPort = hMsg->nPort;
	sysmode = hMsg->sysmode;

	Log3("THIS IS SZ'S SEARCH PROC GET DEVICE IP:[%s] NAME:[%s] TYPE:[%d] ID:[%s] MAC:[%s]",
		hMsg->szIpAddr,
		hMsg->szDevName,
		sysmode,
		hMsg->dwDeviceID,
		szTemp
		);

	GET_LOCK( &g_CallbackContextLock );


	m_envLocal->CallVoidMethod(
		g_CallBack_Handle,
		g_CallBack_SearchResults,
		1,
		jmac,
		jdevname,
		jdeviceid,
		jipaddr,
		nPort,
		sysmode
	);
    
    m_envLocal->DeleteLocalRef(jipaddr);
    m_envLocal->DeleteLocalRef(jdevname);
    m_envLocal->DeleteLocalRef(jmac);
    m_envLocal->DeleteLocalRef(jdeviceid);
    
	PUT_LOCK( &g_CallbackContextLock );

	return;
}

void CSearchDVS::ProcMessageGoke(const char * lpMsg){
	char szTemp[128] = {0};

    if(g_CallBack_Handle == NULL && g_CallBack_SearchResults == NULL){
		return;
    }
		
	jstring jdevname = NULL;
	jstring jipaddr = NULL;
	jstring jdeviceid = NULL;
    jstring jmac = NULL;
    
	int nPort=0;
	int sysmode=0;

	JBNV_SERVER_MSG_DATA_EX * hMsg = (JBNV_SERVER_MSG_DATA_EX*)(lpMsg);

	memset(szTemp, 0, sizeof(szTemp));

	sprintf(szTemp, "%x:%x:%x:%x:%x:%x", hMsg->jbServerPack.bMac[0],
		hMsg->jbServerPack.bMac[1],
		hMsg->jbServerPack.bMac[2],
		hMsg->jbServerPack.bMac[3],
		hMsg->jbServerPack.bMac[4],
		hMsg->jbServerPack.bMac[5]
	);

	jdevname = m_envLocal->NewStringUTF(hMsg->jbServerPack.jspack.szServerName);
	jipaddr = m_envLocal->NewStringUTF(hMsg->jbServerPack.jspack.szIp);
	jdeviceid = m_envLocal->NewStringUTF(hMsg->jbServerPack.csServerNo);
    jmac = m_envLocal->NewStringUTF(szTemp);

	nPort = hMsg->jbServerPack.jspack.wWebPort;
	sysmode = hMsg->jbServerPack.jspack.dwDeviceType;

	Log3("THIS IS GK'S SEARCH PROC GET DEVICE IP:[%s] NAME:[%s] TYPE:[%d] ID:[%s] MAC:[%s]",
		hMsg->jbServerPack.jspack.szIp,
		hMsg->jbServerPack.jspack.szServerName,
		sysmode,
		hMsg->jbServerPack.csServerNo,
		szTemp
		);

	GET_LOCK( &g_CallbackContextLock );

	
	m_envLocal->CallVoidMethod(
		g_CallBack_Handle,
		g_CallBack_SearchResults,
		1,
		jmac,
		jdevname,
		jdeviceid,
		jipaddr,
		nPort,
		sysmode
	);
    
    m_envLocal->DeleteLocalRef(jipaddr);
    m_envLocal->DeleteLocalRef(jdevname);
    m_envLocal->DeleteLocalRef(jmac);
    m_envLocal->DeleteLocalRef(jdeviceid);
    
	PUT_LOCK( &g_CallbackContextLock );

	return;
}

//命令信息处理
void CSearchDVS::OnMessageProc(const char *pszBuffer, int iBufferSize, const char *pszIp)
{
	if(iBufferSize <= 4){
		Log3("SEARCH PROC GET INVLAID SIZE.");
		return;
	}

	short flag = *((short*)(pszBuffer));
	short code = *((short*)(pszBuffer + 2));
	JBNV_SERVER_MSG_DATA_EX * hMsg = (JBNV_SERVER_MSG_DATA_EX *)pszBuffer;

	if(hMsg->dwPackFlag == SERVER_PACK_FLAG){
		ProcMessageGoke(pszBuffer);
	}else if(flag == STARTCODE && code == CMD_GET_RESPONSE){
		ProcMessageShenZhou(pszBuffer + 4);
	}else{
		Log3("Invalid message head:[%04x][%04x][%04x].\n",
			hMsg->dwPackFlag,
			flag,
			code
			);
	}

	return;
}


