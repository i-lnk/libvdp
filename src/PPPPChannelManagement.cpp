
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
	int i = 0;
	for(i;i<MAX_PPPP_CHANNEL_NUM;i++){
		memset(&sessionList[i],0,sizeof(T_CLIENT_SESSION));
		INT_LOCK(&sessionList[i].lock);
	}

	INT_LOCK(&sessionCreateLock);

	InitOpenXL();
}

CPPPPChannelManagement::~CPPPPChannelManagement()
{    
	CloseAll();
	int i = 0;
	for(i;i<MAX_PPPP_CHANNEL_NUM;i++){
		DEL_LOCK(&sessionList[i].lock);
	}

	DEL_LOCK(&sessionCreateLock);
}

int CPPPPChannelManagement::Start(char * szDID, char * user, char * pwd,char * server,char * connectionType)
{
	if(szDID == NULL) return 0;

	int r = 1;
    int i = 0;

	GET_LOCK(&sessionCreateLock);
	
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
        if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
        }
		PUT_LOCK(&sessionList[i].lock);
    }

    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(sessionList[i].deviceID[0] == 0){
			strcpy(sessionList[i].deviceID,szDID);
			sessionList[i].session = new CPPPPChannel(szDID,user,pwd,server,connectionType);
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
    }

	PUT_LOCK(&sessionCreateLock);

	return -1;

jumpout:
    PUT_LOCK(&sessionList[i].lock);
	PUT_LOCK(&sessionCreateLock);

    LogX("start session with list index:[%d].",i);
    
	r = sessionList[i].session->Start(user, pwd, server);

	return r;
}

int CPPPPChannelManagement::Close(char * szDID)
{
	if(szDID == NULL) return 0;

    int i;
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
    	GET_LOCK(&sessionList[i].lock);
        LogX("close session with uuid:[%s] cmp:[%s] list index:[%d].",sessionList[i].deviceID,szDID,i);
        if(strcmp(sessionList[i].deviceID,szDID) == 0){            
			if(sessionList[i].session){
				delete(sessionList[i].session);
				sessionList[i].session = NULL;
			}
			sessionList[i].deviceID[0] = 0;
			PUT_LOCK(&sessionList[i].lock);
            return 1;
        }
		PUT_LOCK(&sessionList[i].lock);
    }
    
    return 0;
}

void CPPPPChannelManagement::CloseAll(){
    int i;
    
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
    	GET_LOCK(&sessionList[i].lock);
        if(sessionList[i].deviceID[0]){
			if(sessionList[i].session){
				delete(sessionList[i].session);
				sessionList[i].session = NULL;
			}
			sessionList[i].deviceID[0] = 0;
        }
		PUT_LOCK(&sessionList[i].lock);
    } 
}

int CPPPPChannelManagement::StartPPPPLivestream(
	char * szDID, 
	char * szURL,
	int audio_sample_rate,
	int	audio_channel,
	int audio_recv_codec,
	int audio_send_codec,
	int video_recv_codec,
	int video_w_crop,
	int video_h_crop
){
  	if(szDID == NULL) return 0;

    int i;
	int e;
	
    for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
    	GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
        }
		PUT_LOCK(&sessionList[i].lock);
    }
    
    return -1;

jumpout:

	e = sessionList[i].session->StartMediaStreams(
					szURL,
					audio_sample_rate,
					audio_channel,
					audio_recv_codec,
					audio_send_codec,
					video_recv_codec,
					video_w_crop,
					video_h_crop
					);

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::ClosePPPPLivestream(char * szDID){
	if(szDID == NULL) return 0;

	int i;
	int e;
	
	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}
	
	return -1;

jumpout:

	e = sessionList[i].session->CloseMediaStreams();

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::GetAudioStatus(char * szDID){

	if(szDID == NULL) return 0;

	int i = 0;
	int e = 0;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	Log3("GET ----> AUDIO PLAY:[%d] AUDIO TALK:[%d].",
				sessionList[i].session->audioEnabled,
				sessionList[i].session->voiceEnabled
				);

	e = (sessionList[i].session->audioEnabled | sessionList[i].session->voiceEnabled << 1) & 0x3;

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::SetAudioStatus(char * szDID,int AudioStatus){

	if(szDID == NULL) return 0;
	
	int i = 0;
	int e = 0;

	int audioEnable = (AudioStatus & 0x1);
	int voiceEnable = (AudioStatus & 0x2) >> 1;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		LogX("SetAudioStatus get lock with index:[%d].",i);
		
		GET_LOCK(&sessionList[i].lock);
		LogX("SetAudioStatus cmp with uuid:[%s][%s].",sessionList[i].deviceID,szDID);
		
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	if(sessionList[i].session == NULL){
		LogX("Invalid session from index:[%d] uuid:[%s].",i,sessionList[i].deviceID);
		PUT_LOCK(&sessionList[i].lock);
		return 0;
	}

	if(voiceEnable){
		sessionList[i].session->MicphoneStart();
	}else{
		sessionList[i].session->MicphoneClose();
	}

	if(audioEnable){
		sessionList[i].session->SpeakingStart();
	}else{
		sessionList[i].session->SpeakingClose();
	}

	Log3("SET ----> AUDIO PLAY:[%d] AUDIO TALK:[%d].",
		sessionList[i].session->audioEnabled,
		sessionList[i].session->voiceEnabled
		);

	e = (sessionList[i].session->audioEnabled | sessionList[i].session->voiceEnabled << 1) & 0x3;

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::SetVideoStatus(char * szDID,int VideoStatus){
	if(szDID == NULL) return 0;

	int i = 0;
	int e = 0;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	if(VideoStatus){
		e = sessionList[i].session->LiveplayStart();
	}else{
		e = sessionList[i].session->LiveplayClose();
	}

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::StartRecorderByDID(char * szDID,char * filepath){
	if(szDID == NULL) return 0;

	int i = 0;
	int e = 0;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	e = sessionList[i].session->RecorderStart(0,0,15,filepath);

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::CloseRecorderByDID(char * szDID)
{
	if(szDID == NULL) return 0;

	int i = 0;
	int e = 0;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	e = sessionList[i].session->RecorderClose();

	PUT_LOCK(&sessionList[i].lock);

	return e;
}

int CPPPPChannelManagement::PPPPIOCmdSend(char * szDID,int type,char * msg,int len)
{   
	if(szDID == NULL) return 0;

	int i = 0;
	int e = 0;

	for(i = 0; i < MAX_PPPP_CHANNEL_NUM; i++){
		GET_LOCK(&sessionList[i].lock);
		if(strcmp(sessionList[i].deviceID,szDID) == 0){
			goto jumpout;
		}
		PUT_LOCK(&sessionList[i].lock);
	}

	return 0;

jumpout:

	e = sessionList[i].session->IOCmdSend(type, msg, len, 0);

	PUT_LOCK(&sessionList[i].lock);

	return e;
}
