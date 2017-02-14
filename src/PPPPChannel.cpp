
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>

#include "utility.h"
#include "PPPPChannel.h"  

#include "IOTCAPIs.h"
#include "IOTCWakeUp.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

#include "object_jni.h"
#include "appreq.h"
#include "apprsp.h"

#define ENABLE_AEC
#define ENABLE_AGC
#define ENABLE_NSX_I
#define ENABLE_NSX_O
#define ENABLE_VAD

#ifdef PLATFORM_ANDROID
#endif
//#define ENABLE_DEBUG

#ifdef PLATFORM_ANDROID

#include <jni.h>
extern JavaVM * g_JavaVM;

#else

static inline int gettid(){ return 0;}

#endif

extern jobject   g_CallBack_Handle;
extern jmethodID g_CallBack_ConnectionNotify;
extern jmethodID g_CallBack_MessageNotify;
extern jmethodID g_CallBack_VideoDataProcess;
extern jmethodID g_CallBack_AlarmNotifyDoorBell;
extern jmethodID g_CallBack_UILayerNotify;

extern COMMO_LOCK g_CallbackContextLock;

COMMO_LOCK OpenSLLock = PTHREAD_MUTEX_INITIALIZER;

unsigned long GetAudioTime(){
#ifdef PLATFORM_ANDROID
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    struct timeval  ts;
    gettimeofday(&ts,NULL);
    return (ts.tv_sec * 1000 + ts.tv_usec / 1000000);
#endif
}

int avSendIOCtrlEx(
	int nAVChannelID, 
	unsigned int nIOCtrlType, 
	const char *cabIOCtrlData, 
	int nIOCtrlDataSize
){
	while(1){
		int ret = avSendIOCtrl(nAVChannelID, nIOCtrlType, cabIOCtrlData, nIOCtrlDataSize);

		if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
//			Log3("avSendIOCtrl is running by other process.");
			usleep(1000); continue;
		}

		return ret;
	}

	return -1;
}


#ifdef PLATFORM_ANDROID

// this callback handler is called every time a buffer finishes recording
static void recordCallback(
	SLAndroidSimpleBufferQueueItf bq, 
	void *context
){
	OPENXL_STREAM * p = (OPENXL_STREAM *)context;
	CPPPPChannel * hPC = (CPPPPChannel *)p->context;

    short *hFrame = p->recordBuffer+(p->iBufferIndex*AEC_CACHE_LEN/2);
	
	hPC->hAudioGetList->Write(hFrame,GetAudioTime());

	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue,(char*)hFrame,AEC_CACHE_LEN);

    p->iBufferIndex = (p->iBufferIndex+1)%CBC_CACHE_NUM;
}

// this callback handler is called every time a buffer finishes playing
static void playerCallback(
	SLAndroidSimpleBufferQueueItf bq, 
	void *context
){
	OPENXL_STREAM * p = (OPENXL_STREAM *)context;
	CPPPPChannel * hPC = (CPPPPChannel *)p->context;

    short *hFrame = p->outputBuffer+(p->oBufferIndex*AEC_CACHE_LEN/2);

	hPC->hAudioPutList->Write((short*)hFrame,GetAudioTime());
	
	int stocksize = hPC->hSoundBuffer->GetStock();

	if(stocksize >= AEC_CACHE_LEN){
//      Log3("read audio data from sound buffer with lens:[%d]",stocksize);
		hPC->hSoundBuffer->Read((char*)hFrame,AEC_CACHE_LEN);
	}else{
        memset((char*)hFrame,0,AEC_CACHE_LEN);
	}

	(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,(char*)hFrame,AEC_CACHE_LEN);

    p->oBufferIndex = (p->oBufferIndex+1)%CBC_CACHE_NUM;
}

#else

static void recordCallback(char * data, int lens, void *context){
    if(lens > AEC_CACHE_LEN){
        Log3("audio record sample is too large:[%d].",lens);
        return;
    }
    
    OPENXL_STREAM * p = (OPENXL_STREAM *)context;
    CPPPPChannel * hPC = (CPPPPChannel *)p->context;
    
    char * pr = (char*)p->recordBuffer;
    memcpy(pr + p->recordSize,data,lens);
    p->recordSize += lens;
    
    if(p->recordSize >= AEC_CACHE_LEN){
        hPC->hAudioGetList->Write((short*)pr,GetAudioTime());
        p->recordSize -= AEC_CACHE_LEN;
        memcpy(pr,pr + AEC_CACHE_LEN,p->recordSize);
    }
}

static void playerCallback(char * data, int lens, void *context){
    if(lens > AEC_CACHE_LEN){
        Log3("audio output sample is too large:[%d].",lens);
        return;
    }
    
    OPENXL_STREAM * p = (OPENXL_STREAM *)context;
    CPPPPChannel * hPC = (CPPPPChannel *)p->context;
    
    int stocksize = hPC->hSoundBuffer->GetStock();
    
    if(stocksize >= lens){
        hPC->hSoundBuffer->Read((char*)data,lens);
    }else{
        memset((char*)data,0,lens);
    }
    
    char * po = (char*)p->outputBuffer;
    memcpy(po + p->outputSize,data,lens);
    p->outputSize += lens;
    
    if(p->outputSize >= AEC_CACHE_LEN){
        hPC->hAudioPutList->Write((short*)po,GetAudioTime());
        p->outputSize -= AEC_CACHE_LEN;
        memcpy(po,
               po + AEC_CACHE_LEN,
               p->outputSize);
    }
}

#endif

void * MeidaCoreProcess(
	void * hVoid
){
	SET_THREAD_NAME("MeidaCoreProcess");
	
	Log3("current thread id is:[%d].",gettid());

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	JNIEnv * hEnv = NULL;
#ifdef PLATFORM_ANDROID
    char isAttached = 0;

	int status = g_JavaVM->GetEnv((void **) &(hEnv), JNI_VERSION_1_4); 
	if(status < 0){ 
		status = g_JavaVM->AttachCurrentThread(&(hEnv), NULL); 
		if(status < 0){
			Log3("iocmd send process AttachCurrentThread Failed!!");
			return NULL;
		}
		isAttached = 1;  
	}
#endif

	int resend = 0;
	int wakeup_times = 20;
    int online_times = 5;

connect:
    hPC->MsgNotify(hEnv, MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECTING);
    
	hPC->speakerChannel = -1;
	hPC->avstremChannel =  0;
	
	Log3("[1:%s]=====>start get free session id for client connection.",
         hPC->szDID);
	
	hPC->sessionID = IOTC_Get_SessionID();
	if(hPC->sessionID < 0){
		Log3("[1:%s]=====>IOTC_Get_SessionID error code [%d]\n",
             hPC->szDID,
             hPC->sessionID);
		sleep(3); 
		goto connect;
	}

	Log3("[2:%s]=====>start connection by uid parallel.",hPC->szDID);
	
	hPC->SID = IOTC_Connect_ByUID_Parallel(hPC->szDID,hPC->sessionID);
	if(hPC->SID < 0){
		Log3("[2:%s]=====>start connection failed with error [%d] with device:[%s]\n",
             hPC->szDID, hPC->SID, hPC->szDID);
		
		switch(hPC->SID){
			case IOTC_ER_UNLICENSE:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_INVALID_ID);
				goto jumperr;
			case IOTC_ER_EXCEED_MAX_SESSION:
			case IOTC_ER_DEVICE_EXCEED_MAX_SESSION:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_EXCEED_SESSION);
				goto jumperr;
            case IOTC_ER_DEVICE_IS_SLEEP:
                if(wakeup_times-- && hPC->deviceStandby){
                    Log3("[2:%s]=====>device is in sleep mode.",hPC->szDID);
                    if(IOTC_WakeUp_WakeDevice(hPC->szDID) < 0){
                        Log3("[2:%s]=====>device not support wakeup function.\n",hPC->szDID);
                    }else{
                        IOTC_Session_Close(hPC->sessionID);
                        goto connect;
                    }
                }
                hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DEVICE_SLEEP);
                hPC->deviceStandby = 1;
                goto jumperr;
			case IOTC_ER_DEVICE_OFFLINE:
                if(online_times--){
                    Log3("[2:%s]=====>device not online,ask again.\n",hPC->szDID);
                    IOTC_Session_Close(hPC->sessionID);
                    sleep(1);
                    goto connect;
                }
			case IOTC_ER_DEVICE_NOT_LISTENING:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DEVICE_NOT_ON_LINE);
				goto jumperr;
			default:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECT_FAILED);
				goto jumperr;
		}
	}
    
    hPC->deviceStandby = 0; // wakeup successful.

	Log3("[3:%s]=====>start av client service with user:[%s] pass:[%s].\n", hPC->szDID, hPC->szUsr, hPC->szPwd);

	if(strlen(hPC->szUsr) == 0 || strlen(hPC->szPwd) == 0){
        
		hPC->MsgNotify(hEnv, MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_NOT_LOGIN);
		
		IOTC_Connect_Stop_BySID(hPC->SID);
		
		Log3("[3:%s]=====>Device can't login by valid user and pass.\n",
             hPC->szDID);
		
		goto jumperr;
	}

	hPC->avIdx = avClientStart2(hPC->SID,
		hPC->szUsr,
		hPC->szPwd, 
		7, 
		&hPC->deviceType, 
		hPC->avstremChannel, 
		&resend
		);
	
	if(hPC->avIdx < 0){
		Log3("[3:%s]=====>avclient start failed:[%d] with free channel:[%d] \n",
             hPC->szDID, hPC->avIdx, hPC->avstremChannel);
		IOTC_Connect_Stop_BySID(hPC->SID);
		
		switch(hPC->avIdx){
			case AV_ER_INVALID_SID:
			case AV_ER_TIMEOUT:
			case AV_ER_REMOTE_TIMEOUT_DISCONNECT:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECT_TIMEOUT);
				break;
			case AV_ER_WRONG_VIEWACCorPWD:
			case AV_ER_NO_PERMISSION:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_INVALID_USER_PWD);
				goto jumperr;
			default:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECT_FAILED);
				goto jumperr;
		}
	
		sleep(3);
		goto connect;
	}

	Log3("[4:%s]=====>session:[%d] idx:[%d] did:[%s] resend:[%d].",
        hPC->szDID,
		hPC->SID,
		hPC->avIdx,
		hPC->szDID,
		resend
		);

	Log3("[5:%s]=====>channel init command proc here.",hPC->szDID);
	hPC->StartIOCmdChannel();
	Log3("[6:%s]=====>channel init command proc done.",hPC->szDID);

	hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS,PPPP_STATUS_ON_LINE);

	GET_LOCK(&hPC->SessionStatusLock);
	hPC->SessionStatus = STATUS_SESSION_IDLE;
	PUT_LOCK(&hPC->SessionStatusLock);

	while(hPC->mediaLinking){
		struct st_SInfo sInfo;
		int ret = IOTC_Session_Check(hPC->SID,&sInfo);
		if(ret < 0){
			hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DISCONNECT);
			Log3("[7:%s]=====>stop old media process start.\n",hPC->szDID);
			hPC->PPPPClose();
    	    hPC->CloseWholeThreads();
			Log3("[7:%s]=====>stop old media process close.\n",hPC->szDID);
			goto connect;
		}
        
		sleep(1);
	}

	hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DISCONNECT);

jumperr:

#ifdef PLATFORM_ANDROID
	if(isAttached) g_JavaVM->DetachCurrentThread();
#endif
    
    hPC->CloseWholeThreads(); // make sure other service thread all exit.
	
	Log3("MediaCoreProcess Exit.");

	GET_LOCK(&hPC->SessionStatusLock);
	hPC->SessionStatus = STATUS_SESSION_DIED;
	PUT_LOCK(&hPC->SessionStatusLock);

	return NULL;	
}

void * IOCmdSendProcess(
	void * hVoid
){
    SET_THREAD_NAME("IOCmdSendProcess");

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
	
	char Cmds[8192] = {0};
	APP_CMD_HEAD * hCmds = (APP_CMD_HEAD*)Cmds;

	int nBytesRead = 0;
	int ret = 0;

	hPC->hIOCmdBuffer->Reset();

    while(hPC->iocmdSending){
		if(hPC->hIOCmdBuffer == NULL){
			Log3("[X:%s]=====>Invalid hIOCmdBuffer.",hPC->szDID);
			sleep(2);
			continue;
		}

		if(hPC->hIOCmdBuffer->GetStock() >= (int)sizeof(APP_CMD_HEAD)){
			
			nBytesRead = hPC->hIOCmdBuffer->Read(hCmds,sizeof(APP_CMD_HEAD));
			if(nBytesRead == sizeof(APP_CMD_HEAD)){
                if(hCmds->Magic != 0x78787878
                || hCmds->CgiLens > sizeof(Cmds)
                || hCmds->CgiLens < 0
                ){
                    Log3("[X:%s]=====>invalid IOCTRL cmds from application. M:[%08X] L:[%d].\n",
                         hPC->szDID,
                         hCmds->Magic,
                         hCmds->CgiLens
                         );
                    
                    hPC->hIOCmdBuffer->Reset();
                }

				nBytesRead = 0;
				while(nBytesRead != hCmds->CgiLens){
					nBytesRead = hPC->hIOCmdBuffer->Read(hCmds->CgiData,hCmds->CgiLens);
					Log3("[X:%s]=====>data not ready.\n",hPC->szDID);
					usleep(1000);
				}
				
				while(hPC->iocmdSending){
					ret = SendCmds(hPC->avIdx,hCmds->AppType,hCmds->CgiData,hPC);
					if(ret == 0){
						break;
					}

//					Log3("[DEV:%s]=====>send IOCTRL cmd failed with error:[%d].",hPC->szDID,ret);

					if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
						usleep(1000); continue;
					}else if(ret == -1){
						Log3("[X:%s]=====>unsupport cmd type:[%d].\n",hPC->szDID,hCmds->AppType);
						break;
					}else{
						break;
					}
				}
			}
		}
		usleep(1000);
    }
	
	Log3("[X:%s]=====>IOCmdSendProcess Exit.",hPC->szDID);

	return NULL;
}

void * IOCmdRecvProcess(
	void * hVoid
){
    SET_THREAD_NAME("IOCmdRecvProcess");

	Log2("current thread id is:[%d].",gettid());

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
    JNIEnv * hEnv = NULL;
    
#ifdef PLATFORM_ANDROID
	char isAttached = 0;

	int status = g_JavaVM->GetEnv((void **) &(hEnv), JNI_VERSION_1_4); 
	if(status < 0){ 
		status = g_JavaVM->AttachCurrentThread(&(hEnv), NULL); 
		if(status < 0){
			Log("iocmd recv process AttachCurrentThread Failed!!");
			return NULL;
		}
		isAttached = 1; 
	}
#endif

	hPC->m_JNIMainEnv = hEnv;

	char Params[32*1024] = {0};
    char MsgStr[4096] = {0};

	unsigned int IOCtrlType = 0;
	CMD_CHANNEL_HEAD * hCCH = (CMD_CHANNEL_HEAD*)Params;
	int avIdx = hPC->avIdx;

    while(hPC->iocmdRecving){
		int ret = avRecvIOCtrl(avIdx,
			&IOCtrlType,
			 hCCH->d,
			 sizeof(Params) - sizeof(CMD_CHANNEL_HEAD),
			 100
			 );

		if(ret < 0){
			switch(ret){
				case AV_ER_DATA_NOREADY:
				case AV_ER_TIMEOUT:
					usleep(1000);
					continue;
			}

			Log3("[X:%s]=====>iocmd get sid:[%d] error:[%d]",hPC->szDID,hPC->SID,ret);
			
			break;
		}

		Log3("[X:%s]=====>iocmd recv frame len:[%d] cmd:[0x%04x].",hPC->szDID,ret,IOCtrlType);

		hCCH->len = ret;
        
        switch(IOCtrlType){
            case IOTYPE_USER_IPCAM_ALARMING_REQ:{
                SMsgAVIoctrlAlarmingReq * hRQ = (SMsgAVIoctrlAlarmingReq *)hCCH->d;
                
                char sTIME[64] = {0};
                char sTYPE[16] = {0};
                
                const char * wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                struct tm stm;
                struct tm * p = &stm;
                
                sprintf(sTYPE,"%d",hRQ->AlarmType);
                
                p = localtime_r((const time_t *)&hRQ->AlarmTime,&stm); //
                if(p == NULL){
                    time_t t;
                    time(&t);
                    p = localtime_r(&t,&stm);
                }
                
                sprintf(sTIME,
                        "%04d-%02d-%02d-%s-%02d-%02d-%02d",
                        1900+p->tm_year,
                        p->tm_mon + 1,
                        p->tm_mday,
                        wday[p->tm_wday],
                        p->tm_hour,
                        p->tm_min,
                        p->tm_sec);
                
                hPC->AlarmNotifyDoorBell(hEnv,
                        hRQ->AlarmDID,
                        sTYPE,
                        sTIME);
                }
                break;
            default:
                Rsp2Json(IOCtrlType, hCCH->d, MsgStr, sizeof(MsgStr));
                break;
        }
        
        Log3("json:[%s].",MsgStr);
        
        GET_LOCK( &g_CallbackContextLock );	
        
        jstring jstring_did = hEnv->NewStringUTF(hPC->szDID);
        jstring jstring_msg = hEnv->NewStringUTF(MsgStr);
        
        hEnv->CallVoidMethod(g_CallBack_Handle,g_CallBack_UILayerNotify,jstring_did,IOCtrlType,jstring_msg);
        
        hEnv->DeleteLocalRef(jstring_did);
        hEnv->DeleteLocalRef(jstring_msg);
        
        PUT_LOCK( &g_CallbackContextLock );	
        
        Log3("[X:%s]=====>call UILayerNotify done by cmd:[%d].\n",hPC->szDID,IOCtrlType);
        
        memset(Params,0,sizeof(Params));
        memset(MsgStr,0,sizeof(MsgStr));
        
        // here we process command for tutk
		// ...
    }

#ifdef PLATFORM_ANDROID
	if(isAttached) g_JavaVM->DetachCurrentThread();
#endif
    
	Log3("[X:%s]=====>iocmd recv proc exit.",hPC->szDID);

	return NULL;
}

static void * VideoPlayProcess(
	void * hVoid
){
	SET_THREAD_NAME("VideoPlayProcess");

	Log2("current thread id is:[%d].",gettid());

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
	JNIEnv * hEnv = NULL;
    
#ifdef PLATFORM_ANDROID
	char isAttached = 0;

	int status = g_JavaVM->GetEnv((void **) &(hEnv), JNI_VERSION_1_4); 
    if(status < 0){ 
        status = g_JavaVM->AttachCurrentThread(&(hEnv), NULL); 
        if(status < 0){
            return NULL;
        }
        isAttached = 1; 
    }
#endif

	jstring    jstring_did = hEnv->NewStringUTF(hPC->szDID);
	jbyteArray jbyteArray_yuv = hEnv->NewByteArray(hPC->YUVSize);
	jbyte *	   jbyte_yuv = (jbyte *)(hEnv->GetByteArrayElements(jbyteArray_yuv,0));

	GET_LOCK(&hPC->DisplayLock);
	hPC->hVideoFrame = NULL;
	PUT_LOCK(&hPC->DisplayLock);

	while(hPC->videoPlaying){

		if(g_CallBack_Handle == NULL || g_CallBack_VideoDataProcess == NULL){
			usleep(1000); continue;
		}


		if(hPC->mediaEnabled != 1){
			usleep(1000); continue;
		}

		GET_LOCK(&hPC->DisplayLock);

		if(hPC->hVideoFrame == NULL){
			PUT_LOCK(&hPC->DisplayLock);
			usleep(1000); continue;
		}
		
		memcpy(jbyte_yuv,hPC->hVideoFrame,hPC->YUVSize);

		hPC->hVideoFrame = NULL;

		PUT_LOCK(&hPC->DisplayLock);

		// put h264 yuv data to java layer
		GET_LOCK(&g_CallbackContextLock);	

		// for yuv draw process
		hEnv->CallVoidMethod(
			g_CallBack_Handle,
			g_CallBack_VideoDataProcess, 
			jstring_did,
			jbyteArray_yuv,
			1, 
			hPC->YUVSize,
			hPC->MW,
			hPC->MH,
			time(NULL));

		PUT_LOCK( &g_CallbackContextLock );
	}

	hEnv->ReleaseByteArrayElements(jbyteArray_yuv,jbyte_yuv,0);
	hEnv->DeleteLocalRef(jbyteArray_yuv);
	hEnv->DeleteLocalRef(jstring_did);

#ifdef PLATFORM_ANDROID
	if(isAttached) g_JavaVM->DetachCurrentThread();
#else
    jbyteArray_yuv = NULL;
    jstring_did = NULL;
#endif
	Log3("video play proc exit.");

	return NULL;
}

static void * VideoRecvProcess(
	void * hVoid
){    
	SET_THREAD_NAME("VideoRecvProcess");

	int ret = 0;

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	FRAMEINFO_t  	frameInfo;
	int 			outBufSize = 0;
	int 			outFrmSize = 0;
	int 			outFrmInfoSize = 0;

	int 			avIdx = hPC->avIdx;

	SMsgAVIoctrlAVStream ioMsg;
	
	memset(&ioMsg,0,sizeof(ioMsg));
	memset(&frameInfo,0,sizeof(frameInfo));

	Log3("START LIVING STREAM WITH SID:[%d] URL:[%s].",
		hPC->SID,
		hPC->szURL
		);

	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_START, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);
	
	if(ret != AV_ER_NoERROR){
		Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
		return NULL;
	}

	Log3("START LIVING STREAM CMD POST DONE.");
	
    int FrmSize = hPC->YUVSize/3;
	AV_HEAD * hFrm = (AV_HEAD*)malloc(FrmSize);
	char    * hYUV = (char*)malloc(hPC->YUVSize);

	if(hFrm == NULL){
		Log3("invalid hFrm ptr address for video frame process.");
		hPC->videoPlaying = 0;
	}

	if(hYUV == NULL){
		Log3("invalid hYUV ptr address for video frame process.");
		hPC->videoPlaying = 0;
	}

	memset(hFrm,0,FrmSize);
	memset(hYUV,0,hPC->YUVSize);

	CH264Decoder * hDec = new CH264Decoder();	

	int firstKeyFrameComming = 0;
	int	isKeyFrame = 0;

	while(hPC->videoPlaying)
	{
		ret = avRecvFrameData2(
			avIdx, 
			hFrm->d,
			FrmSize - sizeof(AV_HEAD),
			&outBufSize, 
			&outFrmSize, 
			(char *)&frameInfo, 
			sizeof(FRAMEINFO_t), 
			&outFrmInfoSize, 
			&hFrm->frameno);

		if(ret < 0){
			switch(ret){
				case AV_ER_LOSED_THIS_FRAME:
				case AV_ER_INCOMPLETE_FRAME:
					Log3("tutk lost frame with error:[%d].",ret);
					firstKeyFrameComming = 0;
					continue;
				case AV_ER_DATA_NOREADY:
					if(firstKeyFrameComming == 0){
//						avSendIOCtrlEx(avIdx, IOTYPE_USER_IPCAM_START, (char *)&ioMsg, sizeof(SMsgAVIoctrlAVStream));
//						Log3("ask for video stream again.");
					}else{
//						Log3("tutk data not ready.");
					}
					continue;
				default:
					Log3("tutk recv frame with error:[%d],avIdx:[%d].",ret,hPC->avIdx);
					break;
			}
			break;
		}

//		Log3("video frame number is:[%d].",hFrm->frameno);

		if(frameInfo.flags == IPC_FRAME_FLAG_IFRAME){
			firstKeyFrameComming = 1;
			isKeyFrame = 1;
		}else{
			isKeyFrame = 0;
		}

		if(firstKeyFrameComming != 1){
//			Log3("waiting for first video key frame coming.\n");
			continue;
		}

		hFrm->type = frameInfo.flags;
		hFrm->len = ret;

		// decode h264 frame
		if(hDec->DecoderFrame((uint8_t *)hFrm->d,hFrm->len,hPC->MW,hPC->MH,isKeyFrame) <= 0){
			Log3("decode h.264 frame failed.");
			firstKeyFrameComming = 0;
			continue;
		}

		if(hPC->avExit == 1){	
			if(hPC->hVideoBuffer->Write(hFrm,hFrm->len + sizeof(AV_HEAD)) == 0){
				Log3("recording buffer is full.may lost frame in mp4.");
			}
		}

		if(TRY_LOCK(&hPC->DisplayLock) != 0){
			continue;
		}
		
		// get h264 yuv data
		hDec->GetYUVBuffer((uint8_t*)hYUV,hPC->YUVSize);
		hPC->hVideoFrame = hYUV;

		PUT_LOCK(&hPC->DisplayLock);
    }

	GET_LOCK(&hPC->DisplayLock);
    
	if(hFrm)   free(hFrm); hFrm = NULL;
	if(hYUV)   free(hYUV); hYUV = NULL;
	if(hDec) delete(hDec); hDec = NULL;
    
    hPC->hVideoFrame = NULL;
    
	PUT_LOCK(&hPC->DisplayLock);
	
	Log3("video recv proc exit.");
	
	return NULL;
}

static void * AudioRecvProcess(
	void * hVoid
){    
	SET_THREAD_NAME("AudioRecvProcess");

	int ret = 0;
	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	FRAMEINFO_t  frameInfo;
	memset(&frameInfo,0,sizeof(frameInfo));

	int avIdx = hPC->avIdx;

	SMsgAVIoctrlAVStream ioMsg;	
	memset(&ioMsg,0,sizeof(ioMsg));

	Log3("audio recv process sid:[%d].",hPC->SID);

	void * hCodec = audio_dec_init(hPC->AudioRecvFormat,8000,1);
	if(hCodec == NULL){
		Log3("initialize audio decodec handle failed.\n");
		return NULL;
	}

	hPC->hAudioBuffer->Reset();
	hPC->hSoundBuffer->Reset();
	
	char Cache[2048] = {0};
	char Codec[8192] = {0};	
	AV_HEAD * hAV = (AV_HEAD*)Cache;
	
	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_AUDIOSTART, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(ret != AV_ER_NoERROR){
		Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
		return NULL;
	}

	void * hAgc = NULL;
	void * hNsx = NULL;

#ifdef ENABLE_AGC
	hAgc = audio_agc_init(
		20,
		2,
		0,
		255,
		8000);

	if(hAgc == NULL){
		Log3("initialize audio agc failed.\n");
		goto jumperr;
	}
#endif

#ifdef ENABLE_NSX_I
	hNsx = audio_nsx_init(2,8000);

	if(hNsx == NULL){
		Log3("initialize audio nsx failed.\n");
		goto jumperr;
	}
#endif
	
	while(hPC->audioPlaying){

		ret = avRecvAudioData(
			avIdx, 
			hAV->d, 
			sizeof(Cache) - sizeof(AV_HEAD), 
			(char *)&frameInfo, 
			sizeof(FRAMEINFO_t), 
			&hAV->frameno
			);

		hAV->len = ret;

		if(ret < 0){
			switch(ret){
				case AV_ER_LOSED_THIS_FRAME:
				case AV_ER_DATA_NOREADY:
					usleep(10);
					continue;
				default:
					Log3("tutk recv frame with error:[%d],avIdx:[%d].",ret,hPC->avIdx);
					break;
			}
			break;
		}

		if(hPC->audioEnabled != 1){
//          Log3("audio pause...");
			continue;
		}

		if(frameInfo.codec_id != hPC->AudioRecvFormat){
			Log3("invalid packet format for audio decoder:[%02X].",frameInfo.codec_id);
			audio_dec_free(hCodec);
			Log3("initialize new audio decoder here.\n")
			hCodec = audio_dec_init(frameInfo.codec_id,8000,1);
			if(hCodec == NULL){
				Log3("initialize audio decodec handle for codec:[%02X] failed.",frameInfo.codec_id);
				break;
			}
			Log3("initialize new audio decoder done.\n")
			hPC->AudioRecvFormat = frameInfo.codec_id;
			continue;
		}
		
		if((ret = audio_dec_process(hCodec,hAV->d,hAV->len,Codec,sizeof(Codec))) < 0){
			
			Log3("audio decodec process run error:%d with codec:[%02X] lens:[%d].\n",
				ret,
				hPC->AudioRecvFormat,
				hAV->len
				);
			
			continue;
		}

		int times = ret/160;
		for(int i = 0; i < times; i++){
#ifdef ENABLE_NSX_I
			audio_nsx_proc(hNsx,&Codec[160*i]);
#endif
#ifdef ENABLE_AGC
			audio_agc_proc(hAgc,&Codec[160*i]);
#endif
			hPC->hAudioBuffer->Write(&Codec[160*i],160); // for audio avi record
			hPC->hSoundBuffer->Write(&Codec[160*i],160); // for audio player callback
		}

//		hPC->hAudioBuffer->Write(Codec,ret); // for audio avi record
//		hPC->hSoundBuffer->Write(Codec,ret); // for audio player callback
	}

#ifdef ENABLE_AGC
	audio_agc_free(hAgc);
#endif
#ifdef ENABLE_NSX_I
	audio_nsx_free(hNsx);
#endif

jumperr:

	audio_dec_free(hCodec);

	Log3("audio recv proc exit.");

	return NULL;
}

static void * AudioSendProcess(
	void * hVoid
){
	SET_THREAD_NAME("AudioSendProcess");

	Log2("current thread id is:[%d].",gettid());

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
	int ret = 0;
	int avIdx = hPC->avIdx;

	SMsgAVIoctrlAVStream ioMsg;	
	memset(&ioMsg,0,sizeof(ioMsg));

	Log3("audio send process sid:[%d].", hPC->SID);

	void * hCodec = audio_enc_init(hPC->AudioSendFormat,8000,1);
	if(hCodec == NULL){
		Log3("initialize audio encodec handle failed.\n");
		return NULL;
	}
    
    Log3("audio send process run with codec:%02x.",hPC->AudioSendFormat);

	Log3("tutk get free channel for speaker.");
	int speakerChannel = IOTC_Session_Get_Free_Channel(hPC->SID);

	ioMsg.channel = speakerChannel;
	int resend = 1;
	int spIdx = -1;

	Log3("5:sid:[%d].",hPC->SID);

tryagain:

	if(!hPC->audioPlaying){
		return NULL;
	}

	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_SPEAKERSTART,
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);
	
	if(ret != AV_ER_NoERROR){
		Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
		return NULL;
	}

	hPC->speakerChannel = speakerChannel;

	Log3("tutk start audio send process by speaker channel:[%d].",speakerChannel);
	spIdx = avServStart(hPC->SID, NULL, NULL,  5, 0, speakerChannel);
//	spIdx = avServStart3(hPC->SID, NULL, 5, 0, speakerChannel, &resend);

	if(spIdx == AV_ER_TIMEOUT){
		Log3("tutk start audio send process timeout, try again.");
		avSendIOCtrlEx(avIdx,IOTYPE_USER_IPCAM_SPEAKERSTOP,(char *)&ioMsg,sizeof(SMsgAVIoctrlAVStream));
		goto tryagain;
	}
	
	if(spIdx < 0){
		Log3("create spearker channel:[%d] for tutk pppp connection failed with error:[%d].",speakerChannel,spIdx);
		return NULL;
	}
	
	Log3("tutk start audio send process by speaker channel:[%d] spIdx:[%d].",speakerChannel,spIdx);

	hPC->spIdx = spIdx;
	
//	avServResetBuffer(spIdx,RESET_ALL,0);
//	avServSetResendSize(spIdx,512);

#ifdef ENABLE_DEBUG
	FILE * hOut = fopen("/mnt/sdcard/vdp-output.raw","wb");
	if(hOut == NULL){
		Log3("initialize audio output file failed.\n");
	}
#endif

#ifdef ENABLE_AEC
	void * hAEC = audio_echo_cancellation_init(3,8000);
#endif

#ifdef ENABLE_NSX_O
	void * hNsx = audio_nsx_init(2,8000);

	if(hNsx == NULL){
		Log3("initialize audio nsx failed.\n");
	}
#endif

#ifdef ENABLE_VAD
	void * hVad = audio_vad_init();
	if(hVad == NULL){
		Log3("initialize audio vad failed.\n");
	}
#endif

	hPC->hAudioPutList = new CAudioDataList(100);
	hPC->hAudioGetList = new CAudioDataList(100);
	if(hPC->hAudioPutList == NULL || hPC->hAudioGetList == NULL){
		Log2("audio data list init failed.");
		hPC->audioPlaying = 0;
	}

	GET_LOCK(&OpenSLLock);

	OPENXL_STREAM * hOSL = NULL;
	hOSL = InitOpenXLStream(
		8000,1,1,hVoid,
		recordCallback,
		playerCallback
		);
	
	if(!hOSL){
		Log3("opensl init failed.");
		hPC->audioPlaying = 0;
	}

	char hFrame[12*AEC_CACHE_LEN] = {0};
	char hCodecFrame[12*AEC_CACHE_LEN] = {0};

	AV_HEAD * hAV = (AV_HEAD*)hFrame;
	char * WritePtr = hAV->d;

	int nBytesNeed = 6*AEC_CACHE_LEN;
	int nVadFrames = 0;

	while(hPC->audioPlaying){
		if(hPC->mediaEnabled != 1){
			usleep(1000); continue;
		}

		if(hPC->hAudioGetList->CheckData() != 1
		|| hPC->hAudioPutList->CheckData() != 1
		){
			usleep(10);
			continue;
		}

		AudioData * hCapture = hPC->hAudioGetList->Read();
		AudioData * hSpeaker = hPC->hAudioPutList->Read();

		if(hCapture == NULL || hSpeaker == NULL){
            Log3("audio data lost...");
			usleep(10);
			continue;
		}

		if(hPC->voiceEnabled != 1){
			usleep(10); 
			continue;
		}

#ifdef ENABLE_AEC
	    short * hAecCapturePCM = hCapture->buf;
		short * hAecSpeakerPCM = hSpeaker->buf;

		if (audio_echo_cancellation_farend(hAEC,(char*)hAecSpeakerPCM,80) != 0){
				Log3("WebRtcAecm_BufferFarend() failed.");
		}
		
		if (audio_echo_cancellation_proc(hAEC,(char*)hAecCapturePCM,(char*)WritePtr,80) != 0){
				Log3("WebRtcAecm_Process() failed.");
		}
#else
		memcpy(WritePtr,hCapture->buf,80*sizeof(short));
#endif

#ifdef ENABLE_NSX_O
		audio_nsx_proc(hNsx,WritePtr);
#endif

#ifdef ENABLE_VAD
		int logration = audio_vad_proc(hVad,WritePtr,80);

        if(logration < 1024){
//			Log3("audio detect vad actived:[%d].\n",logration);
			nVadFrames ++;
		}else{
			nVadFrames = 0;
		}
#endif

		hAV->len += AEC_CACHE_LEN;
		WritePtr += AEC_CACHE_LEN;

		if(hAV->len < nBytesNeed){
			continue;
		}

#ifdef ENABLE_DEBUG
		fwrite(hAV->d,hAV->len,1,hOut);
#endif


#ifdef ENABLE_VAD
		if(nVadFrames > 300){
			Log3("audio detect vad actived.\n");
            hAV->len = 0;
            WritePtr = hAV->d;
            continue;
		}
#endif

		ret = audio_enc_process(hCodec,hAV->d,hAV->len,hCodecFrame,sizeof(hCodecFrame));
		if(ret < 2){
			Log3("audio encode failed with error:%d.\n",ret);
			hAV->len = 0;
			WritePtr = hAV->d;
			continue;
		}
		
//		struct timeval tv = {0,0};
//		gettimeofday(&tv,NULL);

		FRAMEINFO_t frameInfo;
		memset(&frameInfo, 0, sizeof(frameInfo));
		frameInfo.codec_id = hPC->AudioSendFormat;
		frameInfo.flags = (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO;
//		frameInfo.timestamp = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
        frameInfo.reserve2 = ret;

		ret = avSendAudioData(spIdx,hCodecFrame,ret,&frameInfo,sizeof(FRAMEINFO_t));
        
//      Log3("avSendAudioData with lens:[%d].", frameInfo.reserve2);

		if(ret == AV_ER_EXCEED_MAX_SIZE){
			avServResetBuffer(spIdx,RESET_AUDIO,0);
			Log3("tutk av server audio buffer is full.");
		}
		
		if(ret != AV_ER_NoERROR){
			Log2("tutk av server send audio data failed.err:[%d].", ret);
			break;
		}

		hAV->len = 0;
		WritePtr = hAV->d;
	}

	Log2("free opensl audio stream.");
	FreeOpenXLStream(hOSL);
	hOSL = NULL;

	audio_enc_free(hCodec);

#ifdef ENABLE_AEC
	audio_echo_cancellation_free(hAEC);
#endif

#ifdef ENABLE_NSX_O
	audio_nsx_free(hNsx);
#endif

#ifdef ENABLE_VAD
	audio_vad_free(hVad);
#endif

#ifdef ENABLE_DEBUG
	if(hOut) fclose(hOut); hOut = NULL;
#endif


	PUT_LOCK(&OpenSLLock);
	
	if(hPC->hAudioPutList) delete hPC->hAudioPutList;
	if(hPC->hAudioGetList) delete hPC->hAudioGetList;

	hPC->hAudioPutList = NULL;
	hPC->hAudioGetList = NULL;

	Log3("audio send proc exit.");

	return NULL;
}

//
// avi recording process
//
void * RecordingProcess(void * Ptr){
	SET_THREAD_NAME("RecordingProcess");

	Log2("current thread id is:[%d].",gettid());

	CPPPPChannel * hClass = (CPPPPChannel*)Ptr;
	if(hClass == NULL){
		Log2("Invalid channel class object.");
		return NULL;
	}

	long long   ts = 0;
	int nBytesRead = 0;
	int nBytesHave = 0;

	AV_HEAD * hFrm = (AV_HEAD*)malloc(128*1024);

	hClass->hAudioBuffer->Reset();
	hClass->hVideoBuffer->Reset();

	hClass->aIdx = 0;
	hClass->vIdx = 0;

	while(hClass->avExit){
		int aBytesHave = hClass->hAudioBuffer->GetStock();
		int vBytesHave = hClass->hVideoBuffer->GetStock();
		
		if(vBytesHave > (int)(sizeof(AV_HEAD))){
			nBytesRead = hClass->hVideoBuffer->Read(hFrm,sizeof(AV_HEAD));
			while((nBytesHave = hClass->hVideoBuffer->GetStock()) < hFrm->len){
				Log3("wait video recording buffer arriver size:[%d] now:[%d].",hFrm->len,nBytesHave);
				usleep(10); continue;
			}
			nBytesRead = hClass->hVideoBuffer->Read(hFrm->d,hFrm->len);
			hClass->WriteRecorder(hFrm->d,hFrm->len,1,hFrm->type,ts);

//			Log3("video frame write size:[%d].\n",hFrm->len);
		}

		if(aBytesHave >= 640){
			nBytesRead = hClass->hAudioBuffer->Read(hFrm->d,640);
			hClass->WriteRecorder(hFrm->d,nBytesRead,0,0,ts);

//			Log3("audio frame write size:[%d].\n",nBytesRead);
		}
	}

	free(hFrm); hFrm = NULL;

	Log3("stop recording process done.");
	
	return NULL;
}

static void * MediaExitProcess(
	void * hVoid
){    
	SET_THREAD_NAME("CheckExitProcess");

	pthread_detach(pthread_self());
	
	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	Log3("stop media process on device by avapi cmd.");
	hPC->SendAVAPICloseIOCtrl();

	Log3("stop video process.");
    if((long)hPC->videoRecvThread != -1) pthread_join(hPC->videoRecvThread,NULL);
	if((long)hPC->videoPlayThread != -1) pthread_join(hPC->videoPlayThread,NULL);
	
	Log3("stop audio process.");
    if((long)hPC->audioRecvThread != -1) pthread_join(hPC->audioRecvThread,NULL);
  	if((long)hPC->audioSendThread != -1) pthread_join(hPC->audioSendThread,NULL);

	Log3("stop recording process.");
	hPC->CloseRecorder();

	Log3("stop media process done.");
	hPC->videoRecvThread = (pthread_t)-1;
	hPC->videoPlayThread = (pthread_t)-1;
	hPC->audioRecvThread = (pthread_t)-1;
	hPC->audioSendThread = (pthread_t)-1;

	GET_LOCK(&hPC->SessionStatusLock);
	hPC->SessionStatus = STATUS_SESSION_IDLE;
	PUT_LOCK(&hPC->SessionStatusLock);

	pthread_exit(0);
}

CPPPPChannel::CPPPPChannel(char *DID, char *user, char *pwd,char *servser){ 
    memset(szDID, 0, sizeof(szDID));
    strcpy(szDID, DID);

    memset(szUsr, 0, sizeof(szUsr));
    strcpy(szUsr, user);

    memset(szPwd, 0, sizeof(szPwd));
    strcpy(szPwd, pwd);    

    memset(szServer, 0, sizeof(szServer));
    strcpy(szServer, servser);
    
    iocmdRecving = 0;
    videoPlaying = 0;
	audioPlaying = 0;

	voiceEnabled = 0;
	audioEnabled = 0;
	mediaEnabled = 0;
	speakEnabled = 1;
    
	mediaCoreThread = (pthread_t)-1;
	iocmdSendThread = (pthread_t)-1;
	iocmdRecvThread = (pthread_t)-1;
	videoPlayThread = (pthread_t)-1;
	videoRecvThread = (pthread_t)-1;
	audioSendThread = (pthread_t)-1;
	audioRecvThread = (pthread_t)-1;

	deviceType = -1;

	avIdx = spIdx = sessionID = -1;

    SID = -1;

	hAudioStream = NULL;
	hVideoStream = NULL;
	hAVFmtOutput = NULL;
    hAVFmtContext = NULL;
    hAVCodecContext = NULL;
	hRecordFile = NULL;

	vIdx = aIdx = 0;

	MW = 1920;
	MH = 1080;
	YUVSize = (MW * MH) + (MW * MH)/2;

	hAudioBuffer = new CCircleBuf();
	hVideoBuffer = new CCircleBuf();
	hSoundBuffer = new CCircleBuf();
	hIOCmdBuffer = new CCircleBuf();
	
    if(!hIOCmdBuffer->Create(COMMAND_BUFFER_SIZE)){
		while(1){
			Log2("create iocmd procssing buffer failed.\n");
		}
	}
	
	if(!hAudioBuffer->Create(512 * 1024)){
		while(1){
			Log2("create audio recording buffer failed.\n");
		}
	}

	if(!hSoundBuffer->Create(512 * 1024)){
		while(1){
			Log2("create sound procssing buffer failed.\n");
		}
	}

	if(!hVideoBuffer->Create((3*1024*1024))){
		while(1){
			Log2("create video recording buffer failed.\n");
		}
	}

	INT_LOCK(&AviDataLock);
	INT_LOCK(&DisplayLock);
	INT_LOCK(&SndplayLock);
	
	INT_LOCK(&SessionStatusLock);

	GET_LOCK(&SessionStatusLock);
	SessionStatus = STATUS_SESSION_DIED;
	PUT_LOCK(&SessionStatusLock);
}

CPPPPChannel::~CPPPPChannel()
{
    Log3("start free class pppp channel:[0] start.");
    Log3("start free class pppp channel:[1] close p2p connection and threads.");
    
    Close();  
    
    Log3("start free class pppp channel:[2] free buffer.");

	hAudioBuffer->Release();
	hVideoBuffer->Release();
	hSoundBuffer->Release();
	hIOCmdBuffer->Release();

	delete(hAudioBuffer);
	delete(hVideoBuffer);
	delete(hSoundBuffer);
	delete(hIOCmdBuffer);

	hIOCmdBuffer = 
	hAudioBuffer = 
	hSoundBuffer = 
	hVideoBuffer = NULL;
    
    Log3("start free class pppp channel:[3] free lock.");

	DEL_LOCK(&AviDataLock);
	DEL_LOCK(&DisplayLock);
	DEL_LOCK(&SndplayLock);

	DEL_LOCK(&SessionStatusLock);
    
    Log3("start free class pppp channel:[4] close done.");
}

void CPPPPChannel::MsgNotify(
    JNIEnv * hEnv,
    int MsgType,
    int Param
){
    GET_LOCK( &g_CallbackContextLock );

    if(g_CallBack_Handle != NULL && g_CallBack_ConnectionNotify!= NULL){
        jstring jsDID = ((JNIEnv *)hEnv)->NewStringUTF(szDID);
        ((JNIEnv *)hEnv)->CallVoidMethod(g_CallBack_Handle, g_CallBack_ConnectionNotify, jsDID, MsgType, Param);
        ((JNIEnv *)hEnv)->DeleteLocalRef(jsDID);
    }

	PUT_LOCK( &g_CallbackContextLock );
}

int CPPPPChannel::PPPPClose()
{
	Log3("close connection by did:[%s] called.",szDID);

	IOTC_Connect_Stop_BySID(sessionID);

	if(SID >= 0 || avIdx >= 0 || spIdx >= 0){
		Log3("close connection with session:[%d] avIdx:[%d] did:[%s].",SID,avIdx,szDID);
		
		avClientExit(SID,avIdx);
		avClientStop(avIdx);	// stop audio and video recv from device
		avServExit(SID,spIdx);	// for avServStart block
		avServStop(spIdx);		// stop audio and video send to device
		IOTC_Session_Close(sessionID); // close client session handle
	}
	
	avIdx = spIdx = speakerChannel = SID = -1;

	return 0;
}

int CPPPPChannel::Start()
{   
    Log3("start pppp connection to device with uuid:[%s].\n",szDID);

	GET_LOCK(&SessionStatusLock);
	if(SessionStatus != STATUS_SESSION_DIED){
		Log3("session status is:[%d],can't start pppp connection.\n",SessionStatus);
		PUT_LOCK(&SessionStatusLock);
		return -1;
	}
	SessionStatus = STATUS_SESSION_START;
	PUT_LOCK(&SessionStatusLock);

	StartMediaChannel();
	
    return 0;
}

void CPPPPChannel::Close()
{
	GET_LOCK(&SessionStatusLock);
	SessionStatus = STATUS_SESSION_CLOSE;
	PUT_LOCK(&SessionStatusLock);

	mediaLinking = 0;
    
    PPPPClose();

	Log3("stop media core thread.");

	if(mediaCoreThread != (pthread_t)-1){
		pthread_join(mediaCoreThread,NULL);
		mediaCoreThread = (pthread_t)-1;
	}else{
		GET_LOCK(&SessionStatusLock);
		SessionStatus = STATUS_SESSION_DIED;
		PUT_LOCK(&SessionStatusLock);
	}
}

int CPPPPChannel::StartMediaChannel()
{

	mediaLinking = 1;
	pthread_create(&mediaCoreThread,NULL,MeidaCoreProcess,(void*)this);
	
	return 1;
}

int CPPPPChannel::StartIOCmdChannel()
{
	iocmdSending = 1;
    iocmdRecving = 1;
	
	pthread_create(&iocmdSendThread,NULL,IOCmdSendProcess,(void*)this);
    pthread_create(&iocmdRecvThread,NULL,IOCmdRecvProcess,(void*)this);
	
    return 1;
}

int CPPPPChannel::StartAudioChannel()
{
    audioPlaying = 1;
	audioEnabled = 1;
	voiceEnabled = 1;
	
	pthread_create(&audioRecvThread,NULL,AudioRecvProcess,(void*)this);
	pthread_create(&audioSendThread,NULL,AudioSendProcess,(void*)this);
	
    return 1;
}

int CPPPPChannel::StartVideoChannel()
{
    videoPlaying = 1;

	int Err = 0;

	Err = pthread_create(
		&videoRecvThread,
		NULL,
		VideoRecvProcess,
		(void*)this);

	if(Err != 0){
		Log3("create video recv process failed.");
		goto jumperr;
	}

	Err = pthread_create(
		&videoPlayThread,
		NULL,
		VideoPlayProcess,
		(void*)this);

	if(Err != 0){
		Log3("create video play process failed.");
		goto jumperr;
	}

    return 1;

jumperr:

	videoPlaying = 0;

	return 0;
}

int CPPPPChannel::SendAVAPIStartIOCtrl(){
	return 0;
}

int CPPPPChannel::SendAVAPICloseIOCtrl(){
	SMsgAVIoctrlAVStream ioMsg;
	memset(&ioMsg,0,sizeof(ioMsg));
	int ret = 0;

	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_STOP, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(ret < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",ret,avIdx);
		return ret;
	}

	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_AUDIOSTOP, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(ret < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",ret,avIdx);
		return ret;
	}

	ioMsg.channel = spIdx;

	ret = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_SPEAKERSTOP, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(ret < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",ret,avIdx);
		return ret;
	}

//	avClientCleanBuf(avIdx);

	return ret;
}

int CPPPPChannel::CloseWholeThreads()
{
    //F_LOG;
	iocmdSending = 0;
    iocmdRecving = 0;
	mediaEnabled = 0;
    audioPlaying = 0;
	videoPlaying = 0;
	avExit = 0;

	Log3("stop iocmd process.");
    if(iocmdSendThread != (pthread_t)-1) pthread_join(iocmdSendThread,NULL);
	iocmdSendThread = (pthread_t)-1;
	if(iocmdRecvThread != (pthread_t)-1) pthread_join(iocmdRecvThread,NULL);
	iocmdRecvThread = (pthread_t)-1;

	Log3("stop video process.");
    if(videoRecvThread != (pthread_t)-1) pthread_join(videoRecvThread,NULL);
	if(videoPlayThread != (pthread_t)-1) pthread_join(videoPlayThread,NULL);
	videoRecvThread = (pthread_t)-1;
	videoPlayThread = (pthread_t)-1;

	Log3("stop audio process.");
    if(audioRecvThread != (pthread_t)-1) pthread_join(audioRecvThread,NULL);
  	if(audioSendThread != (pthread_t)-1) pthread_join(audioSendThread,NULL);
	audioRecvThread = (pthread_t)-1;
	audioSendThread = (pthread_t)-1;

	Log3("stop recording process.");
	CloseRecorder();

	Log3("stop media thread done.");

	return 0;

}

int CPPPPChannel::CloseMediaStreams(
){
	GET_LOCK(&SessionStatusLock);
	if(SessionStatus != STATUS_SESSION_PLAYING){
		Log3("session status is:[%d],can't close living stream.\n",SessionStatus);
		PUT_LOCK(&SessionStatusLock);
		return -1;
	}
	SessionStatus = STATUS_SESSION_CLOSE_PLAY;
	PUT_LOCK(&SessionStatusLock);

	mediaEnabled = 0;
	videoPlaying = 0;
	audioPlaying = 0;

	Log3("audio stop speaker service with sid:%d iotc-channel:%d sp-idx:%d.",SID,speakerChannel,spIdx);
	if(speakerChannel >= 0) avServExit(SID,speakerChannel);
	if(spIdx >= 0) avServStop(spIdx);
	speakerChannel = -1;
	spIdx = -1;		
	pthread_t tid;
	pthread_create(&tid,NULL,MediaExitProcess,(void*)this);

	return 0;
}
	
int CPPPPChannel::StartMediaStreams(
	const char * url,
	int audio_recv_codec,
	int audio_send_codec,
	int video_recv_codec
){    
    //F_LOG;	
	int ret = 0;
     
    if(SID < 0) return -1;

	struct st_SInfo sInfo;

	int ret = IOTC_Session_Check(SID,&sInfo);
	if(ret < 0){
		MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DISCONNECT);
		Log3("[7:%s]=====>stop old media process start.\n"szDID);
		PPPPClose();
	    CloseWholeThreads();
		Log3("[7:%s]=====>stop old media process close.\n",szDID);
	}

	GET_LOCK(&SessionStatusLock);
	if(SessionStatus != STATUS_SESSION_IDLE){
		Log3("session status is:[%d],can't start living stream.\n",SessionStatus);
		ret = SessionStatus;
		PUT_LOCK(&SessionStatusLock);
		goto jumperr;
	}
	SessionStatus = STATUS_SESSION_PLAYING;
	PUT_LOCK(&SessionStatusLock);

	Log3("media stream start here.");

	mediaEnabled = 1;

	// pppp://usr:pwd@livingstream:[channel id]
	// pppp://usr:pwd@replay/mnt/sdcard/replay/file

	avClientCleanBuf(avIdx);
	memset(szURL,0,sizeof(szURL));

	AudioRecvFormat = audio_recv_codec;
	AudioSendFormat = audio_send_codec;
	VideoRecvFormat = video_recv_codec;

	if(url != NULL){
		memcpy(szURL,url,strlen(url));
	}

	Log3("channel init audio proc.");
	StartAudioChannel();
	Log3("channel init video proc.");
    StartVideoChannel();

	ret = 0;

	return 0;
	
jumperr:

	return ret;
}

int CPPPPChannel::SetSystemParams(int type,char * msg,int len)
{
	char AppCmds[2048] = {0};
	
	APP_CMD_HEAD * hCmds = (APP_CMD_HEAD*)AppCmds;

    hCmds->Magic = 0x78787878;
	hCmds->AppType = type;
	hCmds->CgiLens = len > 0 ? len : (int)strlen(msg);
	
	memcpy(hCmds->CgiData,msg,hCmds->CgiLens);

	return hIOCmdBuffer->Write(AppCmds,sizeof(APP_CMD_HEAD) + hCmds->CgiLens);
}

void CPPPPChannel::AlarmNotifyDoorBell(JNIEnv* hEnv,char *did, char *type, char *time )
{

	if( g_CallBack_Handle != NULL && g_CallBack_AlarmNotifyDoorBell != NULL )
	{
		jstring jdid	   = hEnv->NewStringUTF( szDID );
		jstring resultDid  = hEnv->NewStringUTF( did );
		jstring resultType = hEnv->NewStringUTF( type );
		jstring resultTime = hEnv->NewStringUTF( time );

		Log3("device msg push to %s with type:[%s] time:[%s].",did,type,time);

		hEnv->CallVoidMethod( g_CallBack_Handle, g_CallBack_AlarmNotifyDoorBell, jdid, resultDid, resultType, resultTime );

		hEnv->DeleteLocalRef( jdid );
		hEnv->DeleteLocalRef( resultDid );
		hEnv->DeleteLocalRef( resultType );
		hEnv->DeleteLocalRef( resultTime );
	}
}

/*
AVFormatContext * hAVFmtContext;
AVOutputFormat  * hAVFmtOutput;
AVStream		* hAudioStream;
AVStream		* hVideoStream;
AVCodecContext  * hAVCodecContext;
char * hRecordFile;
*/

int CPPPPChannel::StartRecorder(
	int 		W,			// \BF\ED\B6\C8
	int 		H,			// \B8ß¶\C8
	int 		FPS,		// Ö¡\C2\CA
	char *		SavePath	// 
){
	int Err = 0;

	AVCodec * hACodec = NULL; 
	AVCodec * hVCodec = NULL;

	GET_LOCK(&AviDataLock);

	if(hAVFmtContext){
		Log3("channel already in recording now.");
		PUT_LOCK(&AviDataLock);
		return -1;
	}
	
	hRecordFile  = (char*)malloc(MAX_PATHNAME_LEN);
	memset(hRecordFile,0,MAX_PATHNAME_LEN);
	memcpy(hRecordFile,SavePath,strlen(SavePath));
	
//	sprintf(hRecordFile,"%s/%d.mp4",SavePath,(int)time(NULL));

	avformat_alloc_output_context2(&hAVFmtContext, NULL, NULL, hRecordFile);
	hAVFmtOutput = hAVFmtContext->oformat;

	if(!hAVFmtOutput){
		Log3("initalize av output format failed, filename:%s.",hRecordFile);
		goto jumperr;
	}

	hAVFmtOutput->video_codec = AV_CODEC_ID_H264;

	strncpy(hAVFmtContext->filename,hRecordFile,sizeof(hAVFmtContext->filename));

	if(hAVFmtOutput->video_codec != AV_CODEC_ID_NONE){
		if(add_video_stream(&sVOs,hAVFmtContext,&hVCodec,hAVFmtOutput->video_codec,W,H,30) < 0){
			Log2("add_video_stream error");
			goto jumperr;
		}
		
		Log2("add_video_stream OK");
	}

	if(hAVFmtOutput->audio_codec != AV_CODEC_ID_NONE){
		if(add_audio_stream(&sAOs,hAVFmtContext,&hACodec,hAVFmtOutput->audio_codec,8000,1) < 0){
			Log2("add_audio_stream error");
			goto jumperr;
		}
		
		Log2("add_audio_stream OK");
	}

	if(open_video(hAVFmtContext,hVCodec,&sVOs,NULL) < 0){
	 	Log2("open_video error");
		goto jumperr;
	}
	
	if(open_audio(hAVFmtContext,hACodec,&sAOs,NULL) < 0){
		Log2("open_audio error");
		goto jumperr;
	}
	
	vCTS = aCTS = 0;		
	vLTS = aLTS = 0;		
	vPTS = aPTS = 0;		
	
	sSTS = 0;				
	vIdx = 0;				
	aIdx = 0;;				

	aLen = 0;

	Err = avio_open(&hAVFmtContext->pb,hRecordFile,AVIO_FLAG_WRITE);
	if(Err < 0){
		Log2("avio_open %s failed with err code:%d.",hRecordFile,Err);
		goto jumperr;
	}

	Err = avformat_write_header(hAVFmtContext,NULL);
	if(Err != 0){
		Log2("avformat_write_header failed with err code:%d.",Err);
		goto jumperr;
	}

	hAudioRecCaches = (char *)malloc(sAOs.st->codec->frame_size*2*2);
	if(hAudioRecCaches == NULL){
		Log2("not enough mem for AV recording.");
		goto jumperr;
	}

	avExit = 1;
	memset(&avProc,0,sizeof(avProc));

	Err = pthread_create(
		&avProc,
		NULL,
		RecordingProcess,
		this);

	if(Err != 0){
		Log2("create av recording process failed.");
		goto jumperr;
	}

	Log3("start recording process done.");

	PUT_LOCK(&AviDataLock);

	return  0;

jumperr:
	if(hAVFmtContext){
		avformat_free_context(hAVFmtContext);
		hAVFmtContext = NULL;
	}

	if(hRecordFile != NULL){
		free(hRecordFile);
	}
	hRecordFile = NULL;

	PUT_LOCK(&AviDataLock);

	return -1;
}

int CPPPPChannel::WriteRecorder(
	const char * 	FrameData,
	int				FrameSize,
	int				FrameCode, 	// audio or video codec [0|1]
	int				FrameType,	// keyframe or not [0|1]
	long long		Timestamp
){
	if(hAVFmtContext == NULL){
//		Log2("invalid av format context for wirte operation.");
		return -1;
	}

/*
	if(FrameType == 1){
		Log3("recording video I frame.");
	}
*/

	if(vIdx == 0 && FrameCode == 1 && FrameType != 1){
		Log3("wait first video key frame for avi recorder.");
		return -1;
	}

	if(vIdx == 0 && FrameCode == 0){
		Log3("wait first video key frame for avi recorder.");
		return -1;
	}

	int Err = 0;
	
//	AVRational sAVRational = {1,1000};
	AVPacket   sAVPacket = {0};
	AVStream * hAVStream = FrameCode ? sVOs.st : sAOs.st;
	
	av_init_packet(&sAVPacket);

	struct timeval tv;
	gettimeofday(&tv,NULL);

//	aCTS = vCTS = tv.tv_sec * 1000 + tv.tv_usec / 1000;
//	if(sSTS == 0) sSTS = aCTS;

	if(FrameCode){
/*
		vPTS = (vCTS - sSTS)/(1000/hAVStream->codec->time_base.den);
		if((vPTS != 0) && (vPTS <= vLTS)){
			vPTS = vPTS + 1;
		}
		vLTS = vPTS;
*/
		sAVPacket.pts = sAVPacket.dts = vIdx;
		sAVPacket.stream_index = hAVStream->index;
		sAVPacket.data = (uint8_t*)FrameData;
		sAVPacket.size = FrameSize;
		sAVPacket.flags |= FrameType ? AV_PKT_FLAG_KEY : 0;
		sAVPacket.duration = 0;
		
		Err = write_frame(hAVFmtContext, &hAVStream->codec->time_base,hAVStream, &sAVPacket);
	}else{
		memcpy(hAudioRecCaches + aLen,FrameData,FrameSize);
		aLen += FrameSize;
		if(aLen >= sAOs.st->codec->frame_size*2){
			write_audio_frame(hAVFmtContext,&sAOs,(uint8_t*)hAudioRecCaches,sAOs.st->codec->frame_size*2,0);
			aLen -= (sAOs.st->codec->frame_size*2);
			memcpy(hAudioRecCaches,hAudioRecCaches+sAOs.st->codec->frame_size*2,aLen);
		}
	}
	
	switch(FrameCode){
		case 0:
			aIdx ++;
			break;
		case 1:
			vIdx ++;
			break;
	}
	
	return 0;
}

int CPPPPChannel::CloseRecorder(){

	GET_LOCK(&AviDataLock);
	
	if(hAVFmtContext == NULL){
		Log2("invlaid av format context for record close.");
		PUT_LOCK(&AviDataLock);
		return -1;
	}

	Log3("wait avi record process exit.");
	avExit = 0;
	pthread_join(avProc,NULL);
	Log3("avi record process exit done.");

	av_write_trailer(hAVFmtContext);

	/*
	int i = 0;
	for(i;i<hAVFmtContext->nb_streams;i++){
		avcodec_free_context(&hAVFmtContext->streams[i]->codec);
		free(hAVFmtContext->streams[i]);
	}
	*/

	if(!(hAVFmtContext->oformat->flags & AVFMT_NOFILE)){
		avio_close(hAVFmtContext->pb);
	}

	close_stream(hAVFmtContext, &sVOs);
    close_stream(hAVFmtContext, &sAOs);

	avformat_free_context(hAVFmtContext);
	hAVFmtContext = NULL;

	if(hRecordFile != NULL) free(hRecordFile);
	hRecordFile = NULL;

	if(hAudioRecCaches != NULL) free(hAudioRecCaches);
	hAudioRecCaches = NULL;

	PUT_LOCK(&AviDataLock);

	return 0;
}


