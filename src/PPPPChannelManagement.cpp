
#ifdef PLATFORM_ANDROID
#include <jni.h>
#endif

#include "utility.h"

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

#include "PPPPChannelManagement.h"

CPPPPChannelManagement::CPPPPChannelManagement()
{
    memset(&m_PPPPChannel, 0 ,sizeof(m_PPPPChannel));

	INT_LOCK( &PPPPChannelLock );
	INT_LOCK( &PPPPCommandLock );
	INT_LOCK( &AudioLock );
    
    InitOpenXL();
}

CPPPPChannelManagement::~CPPPPChannelManagement()
{    
    StopAll();

	DEL_LOCK( &PPPPChannelLock );
	DEL_LOCK( &PPPPCommandLock );
	DEL_LOCK( &AudioLock );
}

int CPPPPChannelManagement::Start(char * szDID, char *user, char *pwd,char *server)
{
	if(szDID == NULL) return 0;

    //F_LOG;
	GET_LOCK( &PPPPChannelLock );

	int r = 1;
	
    int i = 0;
	
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
            SAFE_DELETE(m_PPPPChannel[i].pPPPPChannel);
            memset(m_PPPPChannel[i].szDID, 0, sizeof(m_PPPPChannel[i].szDID));
            strcpy(m_PPPPChannel[i].szDID, szDID);
			m_PPPPChannel[i].pPPPPChannel = new CPPPPChannel(szDID, user, pwd, server);
            m_PPPPChannel[i].pPPPPChannel->Start();
            goto jumpout;
        }
    }

    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 0)
        {
            m_PPPPChannel[i].bValid = 1;            
            strcpy(m_PPPPChannel[i].szDID, szDID);      
            m_PPPPChannel[i].pPPPPChannel = new CPPPPChannel(szDID, user, pwd, server);
            m_PPPPChannel[i].pPPPPChannel->Start();
			goto jumpout;
        }
    }

	r = 0;

jumpout:

	PUT_LOCK( &PPPPChannelLock );
    
    return r;
}

int CPPPPChannelManagement::Stop(char * szDID)
{
	if(szDID == NULL) return 0;

	GET_LOCK( &PPPPChannelLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {            
            memset(m_PPPPChannel[i].szDID, 0, sizeof(m_PPPPChannel[i].szDID));
            SAFE_DELETE(m_PPPPChannel[i].pPPPPChannel);       
            m_PPPPChannel[i].bValid = 0;
			PUT_LOCK( &PPPPChannelLock );
            
            return 1;
        }
    }

	PUT_LOCK( &PPPPChannelLock );
    
    return 0;
}

void CPPPPChannelManagement::StopAll(){
	
    GET_LOCK( &PPPPChannelLock );

    int i;
    
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1)
        {
            memset(m_PPPPChannel[i].szDID, 0, sizeof(m_PPPPChannel[i].szDID));
            SAFE_DELETE(m_PPPPChannel[i].pPPPPChannel);           
            m_PPPPChannel[i].bValid = 0;
        }
    } 

	PUT_LOCK( &PPPPChannelLock );
}

int CPPPPChannelManagement::StartPPPPLivestream(
	char * szDID, 
	char * szURL,
	int audio_recv_codec,
	int audio_send_codec,
	int video_recv_codec
){
  	if(szDID == NULL) return 0;

	int ret  = -1;
	
	GET_LOCK( &PPPPChannelLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
        	Log3("start connection with did:[%s].",szDID);
            ret = m_PPPPChannel[i].pPPPPChannel->StartMediaStreams(szURL,audio_recv_codec,audio_send_codec,video_recv_codec);
			break;
        }
    }

	PUT_LOCK( &PPPPChannelLock );
    
    return ret;
}

int CPPPPChannelManagement::ClosePPPPLivestream(char * szDID){

	if(szDID == NULL) return 0;

   	int ret  = -1;

	GET_LOCK( &PPPPChannelLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
           	ret = m_PPPPChannel[i].pPPPPChannel->CloseMediaStreams();
            break;
        }
    }  

	PUT_LOCK( &PPPPChannelLock );
    
    return ret;
}

int CPPPPChannelManagement::GetAudioStatus(char * szDID){

	if(szDID == NULL) return 0;

	GET_LOCK( &PPPPChannelLock );
	GET_LOCK( &AudioLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
    	if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
			Log3("GET ----> AUDIO PLAY:[%d] AUDIO TALK:[%d].",
				m_PPPPChannel[i].pPPPPChannel->audioEnabled,
				m_PPPPChannel[i].pPPPPChannel->voiceEnabled
				);

			int val = (m_PPPPChannel[i].pPPPPChannel->audioEnabled | m_PPPPChannel[i].pPPPPChannel->voiceEnabled << 1) & 0x3;

			PUT_LOCK( &AudioLock );
			PUT_LOCK( &PPPPChannelLock );
		
        	return val;
    	}
    }

	PUT_LOCK( &AudioLock );
	PUT_LOCK( &PPPPChannelLock );

	return 0;
}

int CPPPPChannelManagement::SetAudioStatus(char * szDID,int AudioStatus){

	if(szDID == NULL) return 0;

	GET_LOCK( &PPPPChannelLock );
	GET_LOCK( &AudioLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
            m_PPPPChannel[i].pPPPPChannel->audioEnabled = (AudioStatus & 0x1);
			m_PPPPChannel[i].pPPPPChannel->voiceEnabled = (AudioStatus & 0x2) >> 1;

			Log3("SET ----> AUDIO PLAY:[%d] AUDIO TALK:[%d].",
				m_PPPPChannel[i].pPPPPChannel->audioEnabled,
				m_PPPPChannel[i].pPPPPChannel->voiceEnabled
				);

			PUT_LOCK( &AudioLock );
			PUT_LOCK( &PPPPChannelLock );
			
            return AudioStatus;
        }
    }  

	PUT_LOCK( &AudioLock );
	PUT_LOCK( &PPPPChannelLock );
   
    return 0;
}

int CPPPPChannelManagement::StartRecorderByDID(char * szDID,char * filepath){
	
    if(szDID == NULL) return 0;

	GET_LOCK( &PPPPChannelLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
            int ret = m_PPPPChannel[i].pPPPPChannel->StartRecorder(640,360,25,filepath);
			PUT_LOCK( &PPPPChannelLock );
            return ret;
        }
    }  

	PUT_LOCK( &PPPPChannelLock );
   
    return 0;
}

int CPPPPChannelManagement::CloseRecorderByDID(char * szDID)
{
    if(szDID == NULL) return 0;

	GET_LOCK( &PPPPChannelLock );

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++)
    {
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0)
        {
            m_PPPPChannel[i].pPPPPChannel->CloseRecorder();
			PUT_LOCK( &PPPPChannelLock );
            return 1;
        }
    }  

	PUT_LOCK( &PPPPChannelLock );
   
    return 0;
}

int CPPPPChannelManagement::PPPPSetSystemParams(char * szDID,int type,char * msg,int len)
{   
	if(szDID == NULL){
		Log3("Invalid uuid for application layer caller");
		return 0;
	}

	GET_LOCK( &PPPPChannelLock );
	GET_LOCK( &PPPPCommandLock );

	int r = 0;
    int i;
	
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		if(NULL == m_PPPPChannel[i].pPPPPChannel){
			Log3("Invalid PPPP Channel Object");
			continue;
		}
		
        if(m_PPPPChannel[i].bValid == 1 && strcmp(m_PPPPChannel[i].szDID, szDID) == 0){
            if(1 == m_PPPPChannel[i].pPPPPChannel->SetSystemParams(type, msg, len)){
            	r = 1; goto jumpout;
            }else{
            	r = 0; goto jumpout;
            }
        }
    }

jumpout:

	PUT_LOCK( &PPPPCommandLock );
	PUT_LOCK( &PPPPChannelLock );
    
    return r;
}
