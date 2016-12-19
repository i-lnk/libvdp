
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

#include "apprsp.h"
#include "appreq.h"
#include "echo_control_mobile.h"
#include "gain_control.h"
#include "noise_suppression_x.h"

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

#include "object_jni.h"

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
extern jmethodID g_Callback_UILayerNotify;

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

// this callback handler is called every time a buffer finishes recording
void bqRecordCallback(
	SLAndroidSimpleBufferQueueItf bq, 
	void *context
){
	OPENXL_STREAM * p = (OPENXL_STREAM *)context;
	CPPPPChannel * hPC = (CPPPPChannel *)p->context;

    short *hFrame = p->recBuffer+(p->inBufIndex*AEC_CACHE_LEN/2);
	
	hPC->hAudioGetList->Write(hFrame,GetAudioTime());

	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue,(char*)hFrame,AEC_CACHE_LEN);

    p->inBufIndex = (p->inBufIndex+1)%CBC_CACHE_NUM;
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(
	SLAndroidSimpleBufferQueueItf bq, 
	void *context
){
	OPENXL_STREAM * p = (OPENXL_STREAM *)context;
	CPPPPChannel * hPC = (CPPPPChannel *)p->context;

    short *hFrame = p->playBuffer+(p->outBufIndex*AEC_CACHE_LEN/2);

	hPC->hAudioPutList->Write((short*)hFrame,GetAudioTime());
	
	int stocksize = hPC->hSoundBuffer->GetStock();

//	Log3("read audio data from sound buffer with lens:[%d]",stocksize);

	if(stocksize >= AEC_CACHE_LEN){
		hPC->hSoundBuffer->Read((char*)hFrame,AEC_CACHE_LEN);
	}else{
		memset((char*)hFrame,0,AEC_CACHE_LEN);
	}

	(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,(char*)hFrame,AEC_CACHE_LEN);

    p->outBufIndex = (p->outBufIndex+1)%CBC_CACHE_NUM;
}

void * audio_agc_init(
	int 	gain,
	int 	mode,
	int 	minLv,
	int 	maxLv,
	int 	sampleRate
){
	WebRtcAgcConfig agcConfig;
	agcConfig.compressionGaindB = gain;
	agcConfig.limiterEnable = 1;
	agcConfig.targetLevelDbfs = 2;
	
	void * hAgc = WebRtcAgc_Create();
	if(hAgc == NULL){
		Log3("create webrtc agc failed.");
		return NULL;
	}
	
	if(WebRtcAgc_Init(hAgc, minLv, maxLv, mode, sampleRate) != 0){
		Log3("failed in WebRtcAgc_Init");
		WebRtcAgc_Free(hAgc);
		return NULL;
	}
	
	if(WebRtcAgc_set_config(hAgc, agcConfig) != 0){
		Log3("failed in WebRtcAgc_set_config");
		return NULL;
	}

	PT_AGC_HANDLE Handle = (PT_AGC_HANDLE)malloc(sizeof(T_AGC_HANLE));
	memset(Handle,0,sizeof(T_AGC_HANLE));
	Handle->hAgc = hAgc;
	Handle->MicLvI = 0;
	Handle->MicLvO = 0;

	return Handle;
}

int audio_agc_proc(
	void * 	hAgc,
	char * 	AudioBuffer,
	int		AudioBufferLens
){
	if(hAgc == NULL) return -1;

	if(AudioBuffer == NULL 
	|| AudioBufferLens <= 0 
	|| AudioBufferLens > 80 * 32){
		Log3("Invalid audio buffer or buffer too large to process.\n");
		return -1;
	}

	PT_AGC_HANDLE Handle = (PT_AGC_HANDLE)hAgc;

	int sample = 80;
	int times = AudioBufferLens / (sample * sizeof(short));
	int i = 0;

	short * I_FrmArray[32] = {0};
	short * O_FrmArray[32] = {0};

	short AgcData[80 * 32] = {0};

	for(i = 0;i < times;i ++){
		I_FrmArray[i] = (short*)(&AudioBuffer[sample*sizeof(short)*i]);
		O_FrmArray[i] = (short*)(&AgcData[sample*i]);
	}

	int IMicLv = 0;
	int OMicLv = 0;
	
	int status = 0;

	unsigned char saturationWarning;

	IMicLv = Handle->MicLvO;
	OMicLv = 0;

	status = WebRtcAgc_Process(Handle->hAgc,
		I_FrmArray,
		times,
		sample,
		O_FrmArray,
		IMicLv,&OMicLv,
		0,
		&saturationWarning
		);
	
	if(status != 0){
		Log3("webrtc agc gain failed:[%d].\n",status);
		return -1;
	}
	
	if(saturationWarning == 1){
		Log3("saturationWarning.\n");
	}
	
	Handle->MicLvO = OMicLv;

	memcpy(AudioBuffer,AgcData,sample*times*sizeof(short));

	return 0;
}

void audio_agc_free(
	void * 	hAgc
){
	if(hAgc == NULL) return;

	PT_AGC_HANDLE Handle = (PT_AGC_HANDLE)hAgc;
	
	if(Handle->hAgc != NULL){
		WebRtcAgc_Free(Handle->hAgc);
	}

	free(hAgc);
}

void * audio_echo_cancellation_init(
	int echoMode,
	int sampleRate
){
	AecmConfig config;
	config.cngMode = true;
	config.echoMode = 3;

	void * hAEC = WebRtcAecm_Create();
	
	if(hAEC == NULL){
		Log3("create webrtc aecm failed.");
		return NULL;
	}

	if(WebRtcAecm_Init(hAEC,sampleRate) < 0){
		Log3("initialize webrtc aecm failed.");
		return NULL;
	}

	if(WebRtcAecm_set_config(hAEC,config) != 0){
		Log3("config webrtc aecm failed.");
		goto jumperr;
	}

	return hAEC;

jumperr:
	
	WebRtcAecm_Free(hAEC);

	return NULL;
}

int audio_echo_cancellation_farend(
	void * 		hAEC,
	char *		audioPlayingBuffer,
	int			frameSize
){
	return WebRtcAecm_BufferFarend(
		hAEC,(const int16_t*)audioPlayingBuffer,frameSize
		);
}

int audio_echo_cancellation_proc(
	void *		hAEC,
	char *		audioCaptureBuffer,
	char *		audioProcessBuffer,
	int			frameSize
){
	return WebRtcAecm_Process(
		hAEC,(const int16_t*)audioCaptureBuffer,NULL,(int16_t*)audioProcessBuffer,frameSize,0
		);
}

void audio_echo_cancellation_free(
	void *		hAEC
){
	WebRtcAecm_Free(hAEC);
}

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

	int resend = 1;

	hPC->MsgNotify(hEnv, MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECTING);

connect:
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
			case IOTC_ER_DEVICE_OFFLINE:
			case IOTC_ER_DEVICE_NOT_LISTENING:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DEVICE_NOT_ON_LINE);
				goto jumperr;
			default:
				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECT_FAILED);
				goto jumperr;
		}
	}

	Log3("[3:%s]=====>start av client service with user:[%s] pass:[%s].\n", hPC->szDID, hPC->szUsr, hPC->szPwd);

	if(strlen(hPC->szUsr) == 0 || strlen(hPC->szPwd) == 0
	){
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

	Log3("[4:%s]=====>start media connection with session:[%d] avIdx:[%d] did:[%s] devType:[%d].",
        hPC->szDID,
		hPC->SID,
		hPC->avIdx,
		hPC->szDID,
		hPC->deviceType
		);

	Log3("[5:%s]=====>channel init command proc here.",hPC->szDID);
	hPC->StartIOCmdChannel();
	Log3("[6:%s]=====>channel init command proc done.",hPC->szDID);

	hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS,PPPP_STATUS_ON_LINE);

	while(hPC->mediaLinking){
		struct st_SInfo sInfo;
		int ret = IOTC_Session_Check(hPC->SID,&sInfo);
		if(ret < 0 || hPC->restartConnectionNow == 1){
			hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DISCONNECT);
			Log3("[7:%s]=====>stop old media process start.\n",hPC->szDID);
			hPC->PPPPClose();
    	    hPC->CloseMediaThreads();
			Log3("[7:%s]=====>stop old media process close.\n",hPC->szDID);
			hPC->restartConnectionNow = 0;
			goto connect;
		}
		sleep(1);
	}

	hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DISCONNECT);

jumperr:

#ifdef PLATFORM_ANDROID
	if(isAttached) g_JavaVM->DetachCurrentThread();
#endif
	
	Log3("MediaCoreProcess Exit.");

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
                    Log3("[X:%s]=====>Invalid IOCTRL cmds from application. M:[%08X] L:[%d].\n",
                         hPC->szDID,
                         hCmds->Magic,
                         hCmds->CgiLens
                         );
                    
                    hPC->hIOCmdBuffer->Reset();
                }
                
				nBytesRead = hPC->hIOCmdBuffer->Read(hCmds->CgiData,hCmds->CgiLens);
				
				while(1){
					ret = SendCmds(hPC->avIdx,hCmds->AppType,hCmds->CgiData,hPC);
					if(ret == 0){
						break;
					}

					Log3("[X:%s]=====>send IOCTRL cmd failed with error:[%d].",hPC->szDID,ret);

					if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
						sleep(1); continue;
					}else if(ret == -1){
						Log3("[X:%s]=====>unsupport cmd type:[%d].\n",hPC->szDID,hCmds->AppType);
						break;
					}else{
						break;
					}
				}
			}
		}
		usleep(10000);
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

			hPC->restartConnectionNow = 1;
			
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
                struct tm * p;
                
                sprintf(sTYPE,"%d",hRQ->AlarmType);
                p = localtime((const time_t *)&hRQ->AlarmTime); //
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
        
        hEnv->CallVoidMethod(g_CallBack_Handle,g_Callback_UILayerNotify,jstring_did,IOCtrlType,jstring_msg);
        
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

	while(1){
		ret = avSendIOCtrl(
			avIdx, 
			IOTYPE_USER_IPCAM_START, 
			(char *)&ioMsg, 
			sizeof(SMsgAVIoctrlAVStream)
			);

		if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
//			Log3("avSendIOCtrl is running by other process.");
			usleep(1000); continue;
		}else if(ret == AV_ER_NoERROR){
			break;
		}else{
			Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
			return NULL;
		}
	}
    
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
//					Log3("tutk lost frame with error:[%d].",ret);
					firstKeyFrameComming = 0;
					continue;
				case AV_ER_DATA_NOREADY:
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
	
	while(1){
		ret = avSendIOCtrl(
			avIdx, 
			IOTYPE_USER_IPCAM_AUDIOSTART, 
			(char *)&ioMsg, 
			sizeof(SMsgAVIoctrlAVStream)
			);

		if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
//			Log3("avSendIOCtrl is running by other process.");
			usleep(1000); continue;
		}else if(ret == AV_ER_NoERROR){
			break;
		}else{
			Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
			return NULL;
		}
	}

	void * hAgc = audio_agc_init(
		20,
		kAgcModeAdaptiveDigital,
		0,
		255,
		8000);

	if(hAgc == NULL){
		Log3("initialize audio agc failed.\n");
		goto jumperr;
	}
	
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

		audio_agc_proc(hAgc,Codec,ret);
		
		hPC->hAudioBuffer->Write(Codec,ret); // for audio avi record
		hPC->hSoundBuffer->Write(Codec,ret); // for audio player callback
	}

	audio_agc_free(hAgc);

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

	Log2("tutk get free channel for speaker.");
	int speakerChannel = IOTC_Session_Get_Free_Channel(hPC->SID);

	ioMsg.channel = speakerChannel;

	Log3("5:sid:[%d].",hPC->SID);

	while(1){
		ret = avSendIOCtrl(
			avIdx, 
			IOTYPE_USER_IPCAM_SPEAKERSTART,
			(char *)&ioMsg, 
			sizeof(SMsgAVIoctrlAVStream)
			);
		
		if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
//			Log3("avSendIOCtrl is running by other process.");
			usleep(1000); continue;
		}else if(ret == AV_ER_NoERROR){
			break;
		}else{
			Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",ret,hPC->SID,avIdx);
			return NULL;
		}
	}

	hPC->speakerChannel = speakerChannel;

	Log3("tutk start audio send process by speaker channel:[%d].",speakerChannel);
	int spIdx = avServStart(hPC->SID,NULL,NULL,  5,0,speakerChannel);
	if(spIdx < 0){
		Log3("create spearker channel:[%d] for tutk pppp connection failed with error:[%d].",speakerChannel,spIdx);
		return NULL;
	}
	Log3("tutk start audio send process by speaker channel:[%d] spIdx:[%d].",speakerChannel,spIdx);

	hPC->spIdx = spIdx;
	
	avServResetBuffer(spIdx,RESET_ALL,0);

	void * hAEC = audio_echo_cancellation_init(3,8000);
	
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
		bqRecordCallback,
		bqPlayerCallback
		);
	
	if(!hOSL){
		Log3("opensl init failed.");
		hPC->audioPlaying = 0;
	}

	char hFrame[12*AEC_CACHE_LEN] = {0};
	char hCodecFrame[ 3*AEC_CACHE_LEN] = {0};

	AV_HEAD * hAV = (AV_HEAD*)hFrame;
	char * WritePtr = hAV->d;

	int nBytesNeed = 6*AEC_CACHE_LEN;

	while(hPC->audioPlaying){
		if(hPC->mediaEnabled != 1){
			usleep(1000); continue;
		}

		if(hPC->voiceEnabled != 1){
			usleep(1000); continue;
		}

		if(hPC->hAudioGetList->CheckData() != 1
		|| hPC->hAudioPutList->CheckData() != 1
		){
			usleep(10);   continue;
		}

		AudioData * hRec = hPC->hAudioGetList->Read();
		AudioData * hRef = hPC->hAudioPutList->Read();

		if(hRec == NULL || hRef == NULL){
			usleep(10);   continue;
		}

		short * hRecPCM = hRec->buf;
		short * hRefPCM = hRef->buf;

	    short * hAecRecPCM = hRecPCM;
		short * hAecRefPCM = hRefPCM;
		short * hAecWritePtr = (short *)WritePtr;

		if (audio_echo_cancellation_farend(hAEC,(char*)hAecRefPCM,80) != 0){
				Log3("WebRtcAecm_BufferFarend() failed.");
		}
		
		if (audio_echo_cancellation_proc(hAEC,(char*)hAecRecPCM,(char*)hAecWritePtr,80) != 0){
				Log3("WebRtcAecm_Process() failed.");
		}

		hAV->len += AEC_CACHE_LEN;
		WritePtr += AEC_CACHE_LEN;

		if(hAV->len < nBytesNeed){
			 continue;
		}

		ret = audio_enc_process(hCodec,hAV->d,hAV->len,hCodecFrame,sizeof(hCodecFrame));
		if(ret < 2){
			Log3("audio encode failed with error:%d.\n",ret);
			hAV->len = 0;
			WritePtr = hAV->d;
			continue;
		}
		
		struct timeval tv = {0,0};
		gettimeofday(&tv,NULL);

		FRAMEINFO_t frameInfo;
		memset(&frameInfo, 0, sizeof(frameInfo));
		frameInfo.codec_id = hPC->AudioSendFormat;
		frameInfo.flags = (AUDIO_SAMPLE_8K << 2) | (AUDIO_DATABITS_16 << 1) | AUDIO_CHANNEL_MONO;
		frameInfo.timestamp = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

		ret = avSendAudioData(spIdx,hCodecFrame,ret,&frameInfo,sizeof(FRAMEINFO_t));
		
		if(ret != AV_ER_NoERROR){
			Log2("tutk av server send audio data failed.err:[%d].",ret);
			break;
		}

		hAV->len = 0;
		WritePtr = hAV->d;
	}

#ifdef PLATFORM_ANDROID
	Log2("free opensl audio stream.");
	FreeOpenSLStream(hOSL);
	hOSL = NULL;
#endif

	audio_enc_free(hCodec);

	audio_echo_cancellation_free(hAEC);

	PUT_LOCK(&OpenSLLock);
	
	if(hPC->hAudioPutList) delete hPC->hAudioPutList;
	if(hPC->hAudioGetList) delete hPC->hAudioGetList;

	hPC->hAudioPutList = NULL;
	hPC->hAudioGetList = NULL;

	Log3("audio send proc exit.");

	return NULL;
}


static void * ProcsExitProcess(
	void * hVoid
){    
	SET_THREAD_NAME("CheckExitProcess");

	pthread_detach(pthread_self());
	
	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	GET_LOCK(&hPC->AVProcsLock);

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

	PUT_LOCK(&hPC->AVProcsLock);

	pthread_exit(0);
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

			Log3("video frame write size:[%d].\n",hFrm->len);
		}

		if(aBytesHave >= 640){
			nBytesRead = hClass->hAudioBuffer->Read(hFrm->d,640);
			hClass->WriteRecorder(hFrm->d,nBytesRead,0,0,ts);

			Log3("audio frame write size:[%d].\n",nBytesRead);
		}
	}

	free(hFrm); hFrm = NULL;

	Log3("stop recording process done.");
	
	return NULL;
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
    restartConnectionNow = 0;

	hAVCodecContext = NULL;
	hAVFmtContext = NULL;
	hAudioStream = NULL;
	hVideoStream = NULL;
	hAVFmtOutput = NULL;
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
	
	if(!hAudioBuffer->Create((3000/20)*320)){
		while(1){
			Log2("create audio recording buffer failed.\n");
		}
	}

	if(!hSoundBuffer->Create((3000/20)*320)){
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
	INT_LOCK(&StartAVLock);
	INT_LOCK(&AVProcsLock);

}

CPPPPChannel::~CPPPPChannel()
{	
    Close();

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

	DEL_LOCK(&AviDataLock);
	DEL_LOCK(&DisplayLock);
	DEL_LOCK(&SndplayLock);
	DEL_LOCK(&StartAVLock);
	DEL_LOCK(&AVProcsLock);
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

	StartMediaChannel();
	
    return 0;
}

void CPPPPChannel::Close()
{
	mediaLinking = 0;
    
    PPPPClose();

	Log3("stop media core thread.");

	if(mediaCoreThread != (pthread_t)-1){
		pthread_join(mediaCoreThread,NULL);
		mediaCoreThread = (pthread_t)-1;
	}

	CloseMediaThreads();
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

	ret = avSendIOCtrl(
		avIdx, 
		IOTYPE_USER_IPCAM_STOP, 
		(char *)&ioMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(ret < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",ret,avIdx);
		return ret;
	}

	ret = avSendIOCtrl(
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

	ret = avSendIOCtrl(
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

void CPPPPChannel::CloseMediaThreads()
{
    //F_LOG;
	iocmdSending = 0;
    iocmdRecving = 0;
	mediaEnabled = 0;
    audioPlaying = 0;
	videoPlaying = 0;
	avExit = 0;

	GET_LOCK(&AVProcsLock);

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

	PUT_LOCK(&AVProcsLock);

}

int CPPPPChannel::CloseMediaStreams(
){
	while(TRY_LOCK(&StartAVLock) != 0){
		Log3("start media process already running,wait for exit.");
		return -1;
	}

	mediaEnabled = 0;
	videoPlaying = 0;
	audioPlaying = 0;

	Log3("audio stop speaker service with sid:%d iotc-channel:%d sp-idx:%d.",SID,speakerChannel,spIdx);
	if(speakerChannel >= 0) avServExit(SID,speakerChannel);
	if(spIdx >= 0) avServStop(spIdx);
	speakerChannel = -1;
	spIdx = -1;		
	pthread_t tid;
	pthread_create(&tid,NULL,ProcsExitProcess,(void*)this);

	CloseRecorder();

	PUT_LOCK(&StartAVLock);

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

	while(TRY_LOCK(&StartAVLock) != 0){
		Log3("start or close media stream already running, wait for exit.");
		return -1;
	}

	Log3("media stream start here.");

	mediaEnabled = 1;

	if(TRY_LOCK(&AVProcsLock) != 0){
		Log3("media stream process is busy.");
		ret = -2;
		goto jumpout;
	}

	if(videoRecvThread != (pthread_t)-1
	|| videoPlayThread != (pthread_t)-1
	|| audioRecvThread != (pthread_t)-1
	|| audioSendThread != (pthread_t)-1
	){
		Log3("media stream thread still running, please call close first.");
		ret = -3;
		goto jumpout;
	}

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

	Log2("channel init video proc.");
    StartVideoChannel();
	Log2("channel init audio proc.");
    StartAudioChannel();

	ret = 0;
	
	PUT_LOCK(&AVProcsLock);
	
jumpout:	
	PUT_LOCK(&StartAVLock);

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

	aCTS = vCTS = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	if(sSTS == 0) sSTS = aCTS;

	if(FrameCode){
		vPTS = (vCTS - sSTS)/(1000/hAVStream->codec->time_base.den);
		if((vPTS != 0) && (vPTS <= vLTS)){
			vPTS = vPTS + 1;
		}
		vLTS = vPTS;

		sAVPacket.pts=sAVPacket.dts = vPTS;
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


