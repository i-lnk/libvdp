
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

#include "libvdp.h"
#include "appreq.h"
#include "apprsp.h"

#define ENABLE_AEC
#define ENABLE_AGC
#define ENABLE_NSX_I
#define ENABLE_NSX_O

//#define ENABLE_VAD
#define ENABLE_AUDIO_RECORD

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

    short * hFrame = p->recordBuffer+(p->iBufferIndex * hPC->Audio10msLength  / sizeof(short));
	
	hPC->hAudioGetList->Put((char*)hFrame,hPC->Audio10msLength);

	(*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue,(char*)hFrame,hPC->Audio10msLength);

    p->iBufferIndex = (p->iBufferIndex+1)%CBC_CACHE_NUM;
}

// this callback handler is called every time a buffer finishes playing
static void playerCallback(
	SLAndroidSimpleBufferQueueItf bq, 
	void *context
){
	OPENXL_STREAM * p = (OPENXL_STREAM *)context;
	CPPPPChannel * hPC = (CPPPPChannel *)p->context;

    short *hFrame = p->outputBuffer+(p->oBufferIndex * hPC->Audio10msLength / sizeof(short));

	hPC->hAudioPutList->Put((char*)hFrame,hPC->Audio10msLength);
	
	int stocksize = hPC->hSoundBuffer->Used();

	if(stocksize >= hPC->Audio10msLength){
		hPC->hSoundBuffer->Get((char*)hFrame,hPC->Audio10msLength);
	}else{
        memset((char*)hFrame,0,hPC->Audio10msLength);
	}

	(*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue,(char*)hFrame,hPC->Audio10msLength);

    p->oBufferIndex = (p->oBufferIndex+1)%CBC_CACHE_NUM;
}

#else

static void recordCallback(char * data, int lens, void *context){
    OPENXL_STREAM * p = (OPENXL_STREAM *)context;
    CPPPPChannel * hPC = (CPPPPChannel *)p->context;

	if(lens > hPC->Audio10msLength){
        Log3("audio record sample is too large:[%d].",lens);
        return;
    }
    
    char * pr = (char*)p->recordBuffer;
    memcpy(pr + p->recordSize,data,lens);
    p->recordSize += lens;
    
    if(p->recordSize >= hPC->Audio10msLength){
        hPC->hAudioGetList->Put(pr,hPC->Audio10msLength);
        p->recordSize -= hPC->Audio10msLength;
        memcpy(pr,pr + hPC->Audio10msLength,p->recordSize);
    }
}

static void playerCallback(char * data, int lens, void *context){
    OPENXL_STREAM * p = (OPENXL_STREAM *)context;
    CPPPPChannel * hPC = (CPPPPChannel *)p->context;

	 if(lens > hPC->Audio10msLength){
        Log3("audio output sample is too large:[%d].",lens);
        return;
    }
    
    int stocksize = hPC->hSoundBuffer->Used();
    
    if(stocksize >= lens){
        hPC->hSoundBuffer->Get((char*)data,lens);
    }else{
        memset((char*)data,0,lens);
    }
    
    char * po = (char*)p->outputBuffer;
    memcpy(po + p->outputSize,data,lens);
    p->outputSize += lens;
    
    if(p->outputSize >= hPC->Audio10msLength){
        hPC->hAudioPutList->Put(po,hPC->Audio10msLength);
        p->outputSize -= hPC->Audio10msLength;
        memcpy(po,
               po + hPC->Audio10msLength,
               p->outputSize);
    }

}

#endif

void * IOCmdSendProcess(
	void * hVoid
){
    SET_THREAD_NAME("IOCmdSendProcess");

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
	
	char Cmds[8192] = {0};
	APP_CMD_HEAD * hCmds = (APP_CMD_HEAD*)Cmds;

	int nBytesRead = 0;
	int ret = 0;

	hPC->hIOCmdBuffer->Clear();

    while(hPC->iocmdSending){
		if(hPC->hIOCmdBuffer == NULL){
			Log3("[X:%s]=====>Invalid hIOCmdBuffer.",hPC->szDID);
			sleep(2);
			continue;
		}

		if(hPC->hIOCmdBuffer->Used() >= (int)sizeof(APP_CMD_HEAD)){
			
			nBytesRead = hPC->hIOCmdBuffer->Get((char*)hCmds,sizeof(APP_CMD_HEAD));
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
                    
                    hPC->hIOCmdBuffer->Clear();
                }

				nBytesRead = 0;
				while(nBytesRead != hCmds->CgiLens){
					nBytesRead = hPC->hIOCmdBuffer->Get(hCmds->CgiData,hCmds->CgiLens);
					Log3("[X:%s]=====>data not ready.\n",hPC->szDID);
					usleep(1000);
				}
				
				while(hPC->iocmdSending){
					switch(hCmds->AppType){
						case IOTYPE_USER_IPCAM_DEL_IOT_REQ:
						case IOTYPE_USER_IPCAM_LST_IOT_REQ:
						case IOTYPE_USER_IPCAM_RAW_REQ: // for byte data request
							ret = avSendIOCtrl(hPC->avIdx,hCmds->AppType,hCmds->CgiData,hCmds->CgiLens);
							break;
						default:
							ret = SendCmds(hPC->avIdx,hCmds->AppType,hCmds->CgiData,hCmds->CgiLens,hPC);
							break;
					}
					
					if(ret == 0){
						break;
					}

					Log3("[DEV:%s]=====>send IOCTRL cmd failed with error:[%d].",hPC->szDID,ret);

					if(ret == AV_ER_SENDIOCTRL_ALREADY_CALLED){
						usleep(1000); 
						continue;
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
	
	Log3("[X:%s]=====>iocmd send proc exit.",hPC->szDID);

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

	unsigned int IOCtrlType = 0;

	int avIdx = hPC->avIdx;
	int jbyteArrayLens = 8192;
	
	char D[2048] = {0};
	char S[8192] = {0};

	CMD_CHANNEL_HEAD * hCCH = (CMD_CHANNEL_HEAD*)D;

	jbyteArray jbyteArray_msg = hEnv->NewByteArray(jbyteArrayLens);

    while(hPC->iocmdRecving){
		int ret = avRecvIOCtrl(avIdx,
			&IOCtrlType,
			 hCCH->d,
			 sizeof(D) - sizeof(CMD_CHANNEL_HEAD),
			 100);

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
			case IOTYPE_USER_IPCAM_DEL_IOT_RESP:
			case IOTYPE_USER_IPCAM_LST_IOT_RESP:
			case IOTYPE_USER_IPCAM_RAW_RESP: // for byte data response
				hEnv->SetByteArrayRegion(jbyteArray_msg, 0, hCCH->len, (const jbyte *)hCCH->d);
				break;
			case IOTYPE_USER_IPCAM_RECORD_PLAYCONTROL_RESP:{
					SMsgAVIoctrlPlayRecordResp * hRQ = (SMsgAVIoctrlPlayRecordResp *)hCCH->d;
					if(hRQ->command == AVIOCTRL_RECORD_PLAY_START){
						hPC->playrecChannel = hRQ->result;
						continue;
					}
				}
				break;
			case IOTYPE_USER_IPCAM_DEVICESLEEP_RESP:{	
					SMsgAVIoctrlSetDeviceSleepResp * hRQ = (SMsgAVIoctrlSetDeviceSleepResp *)hCCH->d;
					if(hRQ->result == 0){
						hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_DEVICE_SLEEP);
						Log3("[X:%s]=====>device sleeping now.\n",hPC->szDID);
					}else{
						Log3("[X:%s]=====>device sleeping failed,still keep online.\n",hPC->szDID);
					}
				}
				break;
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
				memset(S,0,sizeof(S));
                ParseResponseForUI(IOCtrlType, hCCH->d, S, jbyteArrayLens);
				hCCH->len = strlen(S);
				hEnv->SetByteArrayRegion(jbyteArray_msg, 0, hCCH->len, (const jbyte *)S);
				Log3("response parse to json is:[%s].",S);
                break;
        }
        
        GET_LOCK( &g_CallbackContextLock );	
        
        jstring jstring_did = hEnv->NewStringUTF(hPC->szDID);
        
        hEnv->CallVoidMethod(
			g_CallBack_Handle,
			g_CallBack_UILayerNotify,
			jstring_did,
			IOCtrlType,
			jbyteArray_msg,
			hCCH->len
			);

		hEnv->DeleteLocalRef(jstring_did);
        
        PUT_LOCK( &g_CallbackContextLock );	
        
        Log3("[X:%s]=====>call UILayerNotify done by cmd:[%d].\n",hPC->szDID,IOCtrlType);
        
        memset(D,0,sizeof(D));
    }

	hEnv->DeleteLocalRef(jbyteArray_msg);

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
	
	unsigned int frameTimestamp = 0;

	GET_LOCK(&hPC->DisplayLock);
	hPC->hVideoFrame = NULL;
	PUT_LOCK(&hPC->DisplayLock);

	while(hPC->mediaLinking){

		if(g_CallBack_Handle == NULL || g_CallBack_VideoDataProcess == NULL){
			usleep(1000); continue;
		}


		if(hPC->videoPlaying != 1){
//			Log3("video play process paused...");
			sleep(1); 
			continue;
		}

		GET_LOCK(&hPC->DisplayLock);

		if(hPC->hVideoFrame == NULL){
			PUT_LOCK(&hPC->DisplayLock);
			usleep(1000); continue;
		}
		
		memcpy(jbyte_yuv,hPC->hVideoFrame,hPC->YUVSize);
		frameTimestamp = hPC->FrameTimestamp;

		hPC->hVideoFrame = NULL;
		
		int NW = hPC->MW;
		int NH = hPC->MH;
		int NS = NW * NH + NW * NH * 3 / 2;

		PUT_LOCK(&hPC->DisplayLock);

		/*
		hPC->hDec->YUV420CUTSIZE(
			(char*)jbyte_yuv,
			(char*)jbyte_yuv,
			hPC->MW,
			hPC->MH,
			NW,
			NH
			);
		*/

		// put h264 yuv data to java layer
		GET_LOCK(&g_CallbackContextLock);	

		// for yuv draw process
		hEnv->CallVoidMethod(
			g_CallBack_Handle,
			g_CallBack_VideoDataProcess,
			jstring_did,
			jbyteArray_yuv,
			1, 
			NS,
			NW,
			NH,
			frameTimestamp);

		PUT_LOCK(&g_CallbackContextLock);
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

static void * VideoRecvProcess(void * hVoid){

	SET_THREAD_NAME("VideoRecvProcess");
	
	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;
	
	int 			ret = 0;
	
	FRAMEINFO_t  	frameInfo;
	int 			outBufSize = 0;
	int 			outFrmSize = 0;
	int 			outFrmInfoSize = 0;
	int 			firstKeyFrameComming = 0;
	int				isKeyFrame = 0;

	unsigned int	server = 0;
	int 			resend = 0;
	int 			avIdx = hPC->avIdx;

	int 			FrmSize = hPC->YUVSize/3;
	AV_HEAD * 		hFrm = (AV_HEAD*)malloc(FrmSize);
	char    * 		hYUV = (char*)malloc(hPC->YUVSize);

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

	hFrm->startcode = 0xa815aa55;
	
	memset(&frameInfo,0,sizeof(frameInfo));
	
	while(hPC->mediaLinking)
	{
		if(hPC->videoPlaying == 0){
//			Log3("video recv process paused...");
			avClientCleanVideoBuf(avIdx);
			avIdx = -1;
			sleep(1); 
			continue;
		}

		// set play index
		if(avIdx < 0){
			avIdx = hPC->rpIdx >= 0 ? hPC->rpIdx : hPC->avIdx;
			Log3("video playing start with idx:[%d] mode:[%s]",
				avIdx,
				hPC->rpIdx >= 0 ? "replay" : "livestream"
				);
			continue;
		}
	
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
					usleep(10000);
					continue;
				default:
//					Log3("tutk recv frame with error:[%d],avIdx:[%d].",ret,hPC->avIdx);
					break;
			}
			continue;
		}

		if(frameInfo.flags == IPC_FRAME_FLAG_IFRAME){
			firstKeyFrameComming = 1;
			isKeyFrame = 1;
		}else{
			isKeyFrame = 0;
		}

		if(firstKeyFrameComming != 1){
			Log3("waiting for first video key frame coming.\n");
			continue;
		}

		hFrm->type = frameInfo.flags;
		hFrm->len = ret;

		int W = 0;
		int H = 0;

		// decode h264 frame
		if(hPC->hDec->DecoderFrame((uint8_t *)hFrm->d,hFrm->len,W,H,isKeyFrame) <= 0){
			Log3("decode h.264 frame failed.");
			firstKeyFrameComming = 0;
			continue;
		}

		if(W <= 0 || H <= 0){
			Log3("invalid decode resolution W:%d H:%d.",W,H);
			continue;
		}

		int nBytesHave = hPC->hVideoBuffer->Available();

		if(hPC->recordingExit){
			if(nBytesHave >= hFrm->len + sizeof(AV_HEAD)){
				hPC->hVideoBuffer->Put((char*)hFrm,hFrm->len + sizeof(AV_HEAD));
			}
		}

		if(TRY_LOCK(&hPC->DisplayLock) != 0){
			continue;
		}

		hPC->MW = W - hPC->MWCropSize;
		hPC->MH = H - hPC->MHCropSize;
		
		// get h264 yuv data
		hPC->hDec->GetYUVBuffer((uint8_t*)hYUV,hPC->YUVSize,hPC->MW,hPC->MH);
		hPC->hVideoFrame = hYUV;
		hPC->FrameTimestamp = frameInfo.timestamp;

		PUT_LOCK(&hPC->DisplayLock);
    }

	GET_LOCK(&hPC->DisplayLock);
    
	if(hFrm)   free(hFrm); hFrm = NULL;
	if(hYUV)   free(hYUV); hYUV = NULL;
    
    hPC->hVideoFrame = NULL;
    
	PUT_LOCK(&hPC->DisplayLock);
    
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

	char Cache[2048] = {0};
	char Codec[4096] = {0};
    
    int  CodecLength = 0;
    int  CodecLengthNeed = 960;
    
	AV_HEAD * hAV = (AV_HEAD*)Cache;

	void * hAgc = NULL;
	void * hNsx = NULL;

	Log3("audio recv process sid:[%d].",hPC->SID);

jump_rst:
	
	if(hPC->mediaLinking == 0) return NULL;
	
	void * hCodec = audio_dec_init(
		hPC->AudioRecvFormat,
		hPC->AudioSampleRate,
		hPC->AudioChannel
		);
	
	if(hCodec == NULL){
//		Log3("initialize audio decodec handle failed.\n");
		sleep(1);
		goto jump_rst;
	}

	hPC->hAudioBuffer->Clear();
	hPC->hSoundBuffer->Clear();

#ifdef ENABLE_AGC
	hAgc = audio_agc_init(
		20,
		2,
		0,
		255,
		hPC->AudioSampleRate);

	if(hAgc == NULL){
		Log3("initialize audio agc failed.\n");
		goto jumperr;
	}
#endif

#ifdef ENABLE_NSX_I
	hNsx = audio_nsx_init(2,hPC->AudioSampleRate);

	if(hNsx == NULL){
		Log3("initialize audio nsx failed.\n");
		goto jumperr;
	}
#endif
	
	while(hPC->mediaLinking){

		if(hPC->audioPlaying == 0){
//			Log3("audio recv process paused...");
			avClientCleanAudioBuf(avIdx);
			hPC->hAudioBuffer->Clear();
			hPC->hSoundBuffer->Clear();
			CodecLength = 0;
			avIdx = -1;
			sleep(1); 
			continue;
		}

		// set play index
		if(avIdx < 0){
			avIdx = hPC->rpIdx >= 0 ? hPC->rpIdx : hPC->avIdx;
			Log3("audio playing start with idx:[%d] mode:[%s]",
				avIdx,
				hPC->rpIdx >= 0 ? "replay" : "livestream"
				);
			continue;
		}
		
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
					usleep(10000);
					continue;
				default:
//					Log3("tutk recv frame with error:[%d],avIdx:[%d].",ret,hPC->avIdx);
					break;
			}
			continue;
		}

		if(frameInfo.codec_id != hPC->AudioRecvFormat){
			Log3("invalid packet format for audio decoder:[%02X].",frameInfo.codec_id);
			audio_dec_free(hCodec);
			
			Log3("initialize new audio decoder here.\n")
				
			hCodec = audio_dec_init(
				frameInfo.codec_id,
				hPC->AudioSampleRate,
				hPC->AudioChannel
				);
			
			if(hCodec == NULL){
				Log3("initialize audio decodec handle for codec:[%02X] failed.",frameInfo.codec_id);
				break;
			}
			Log3("initialize new audio decoder done.\n")
			hPC->AudioRecvFormat = frameInfo.codec_id;
			continue;
		}
		
		if((ret = audio_dec_process(
                hCodec,
                hAV->d,
                hAV->len,
                &Codec[CodecLength],
                sizeof(Codec) - CodecLength)) < 0){
			
			Log3("audio decodec process run error:%d with codec:[%02X] lens:[%d].\n",
				ret,
				hPC->AudioRecvFormat,
				hAV->len
				);
			
			continue;
		}
        
        CodecLength += ret;
        
        if(CodecLength < CodecLengthNeed){
            continue;
        }

		int Round = CodecLengthNeed/hPC->Audio10msLength;
        
		for(int i = 0; i < Round; i++){
#ifdef ENABLE_NSX_I
			audio_nsx_proc(hNsx,&Codec[hPC->Audio10msLength*i],hPC->Audio10msLength);
#endif
#ifdef ENABLE_AGC
			audio_agc_proc(hAgc,&Codec[hPC->Audio10msLength*i],hPC->Audio10msLength);
#endif
			if(hPC->audioEnabled){
				hPC->hSoundBuffer->Put((char*)&Codec[hPC->Audio10msLength*i],hPC->Audio10msLength); // for audio player callback
			}
			
#ifdef ENABLE_AUDIO_RECORD
			hPC->hAudioBuffer->Put((char*)&Codec[hPC->Audio10msLength*i],hPC->Audio10msLength); // for audio avi record
#endif
		}
        
        CodecLength -= CodecLengthNeed;
        memcpy(Codec,&Codec[CodecLengthNeed],CodecLength);

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

	void * hCodec = NULL;
	void * hAEC = NULL;
	void * hNsx = NULL;
	void * hVad = NULL;
	OPENXL_STREAM * hOSL = NULL;

	SMsgAVIoctrlAVStream ioMsg;	
	memset(&ioMsg,0,sizeof(ioMsg));

	Log3("audio send process sid:[%d].", hPC->SID);

#ifdef ENABLE_DEBUG
	FILE * hOut = fopen("/mnt/sdcard/vdp-output.raw","wb");
	if(hOut == NULL){
		Log3("initialize audio output file failed.\n");
	}
#endif

jump_rst:
	if(hPC->mediaLinking == 0) return NULL;

	hCodec = audio_enc_init(hPC->AudioSendFormat,hPC->AudioSampleRate,hPC->AudioChannel);
	if(hCodec == NULL){
//		Log3("initialize audio encodec handle failed.\n");
		sleep(1);
		goto jump_rst;
	}
    
    Log3("audio send process run with codec:%02x.",hPC->AudioSendFormat);


#ifdef ENABLE_AEC
	hAEC = audio_echo_cancellation_init(3,hPC->AudioSampleRate);
	if(hAEC == NULL){
		Log3("initialize audio aec failed.\n");
	}
#endif

#ifdef ENABLE_NSX_O
	hNsx = audio_nsx_init(2,hPC->AudioSampleRate);
	if(hNsx == NULL){
		Log3("initialize audio nsx failed.\n");
	}
#endif

#ifdef ENABLE_VAD
	hVad = audio_vad_init();
	if(hVad == NULL){
		Log3("initialize audio vad failed.\n");
	}
#endif

wait_next:
	
	if(hPC->mediaLinking == 0) return NULL;

	if(hPC->spIdx < 0){
//		Log3("tutk start audio send process with error:[%d] try again.",hPC->spIdx);
		sleep(1);
		goto wait_next;
	}
	
	Log3("tutk start audio send process by speaker channel:[%d] spIdx:[%d].",
		hPC->speakerChannel,
		hPC->spIdx);

	if(hPC->hAudioPutList) delete hPC->hAudioPutList;
	if(hPC->hAudioGetList) delete hPC->hAudioGetList;

	hPC->hAudioPutList = new CCircleBuffer(32,hPC->Audio10msLength,0);
	hPC->hAudioGetList = new CCircleBuffer(32,hPC->Audio10msLength,0);
	
	if(hPC->hAudioPutList == NULL || hPC->hAudioGetList == NULL){
		Log2("audio data list init failed.");
		hPC->audioPlaying = 0;
	}

	Log3("=====> opensl init start.\n");
	
	hOSL = InitOpenXLStream(
		hPC->AudioSampleRate,
		hPC->AudioChannel,
		hPC->AudioChannel,
		hVoid,
		recordCallback,
		playerCallback
		);

	Log3("=====> opensl init close.\n");
	
	if(!hOSL){
		Log3("opensl init failed.");
	}

	char hFrame[2*960] = {0};
	char hCodecFrame[2*960] = {0};

	AV_HEAD * hAV = (AV_HEAD*)hFrame;
	char * WritePtr = hAV->d;

	int nBytesNeed = 960;	// max is 960 for opus encoder process.
	int nVadFrames = 0;

	char speakerData[320] = {0};
	char captureData[320] = {0};

	while(hPC->mediaLinking){

		if(hPC->audioPlaying == 0){
			if(hOSL){ 
				FreeOpenXLStream(hOSL);
				hOSL = NULL;
			}
			goto wait_next;	// wait next audio receiver from device
		}


		int captureLens = hPC->hAudioGetList->Used();
		int speakerLens = hPC->hAudioPutList->Used();

		if(captureLens < hPC->Audio10msLength || speakerLens < hPC->Audio10msLength){
			usleep(10000);
			continue;
		}

		hPC->hAudioGetList->Get(captureData,hPC->Audio10msLength);
		hPC->hAudioPutList->Get(speakerData,hPC->Audio10msLength);

		if(hPC->voiceEnabled != 1){
			usleep(10000); 
			continue;
		}

#ifdef ENABLE_AEC
		if (audio_echo_cancellation_farend(hAEC,(char*)speakerData,hPC->Audio10msLength/sizeof(short)) != 0){
			Log3("WebRtcAecm_BufferFarend() failed.");
		}
		
		if (audio_echo_cancellation_proc(hAEC,(char*)captureData,(char*)WritePtr,hPC->Audio10msLength/sizeof(short)) != 0){
				Log3("WebRtcAecm_Process() failed.");
		}
#else
		memcpy(WritePtr,captureData,hPC->Audio10msLength);
#endif

#ifdef ENABLE_NSX_O
		audio_nsx_proc(hNsx,WritePtr,hPC->Audio10msLength);
#endif

#ifdef ENABLE_VAD
		int logration = audio_vad_proc(hVad,WritePtr,hPC->Audio10msLength);

        if(logration < 1024){
//			Log3("audio detect vad actived:[%d].\n",logration);
			nVadFrames ++;
		}else{
			nVadFrames = 0;
		}
#endif

		hAV->len += hPC->Audio10msLength;
		WritePtr += hPC->Audio10msLength;

		if(hAV->len < nBytesNeed){
			continue;
		}

#ifdef ENABLE_DEBUG
		fwrite(hAV->d,hAV->len,1,hOut);
#endif

#ifdef ENABLE_VAD
		if(nVadFrames > 300){
//			Log3("audio detect vad actived.\n");
            hAV->len = 0;
            WritePtr = hAV->d;
            continue;
		}
#endif
		ret = audio_enc_process(
            hCodec,
            hAV->d,
            hAV->len,
            hCodecFrame,
            sizeof(hCodecFrame));
        
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

		ret = avSendAudioData(hPC->spIdx,hCodecFrame,ret,&frameInfo,sizeof(FRAMEINFO_t));

		switch(ret){
			case AV_ER_NoERROR:
				break;
			case AV_ER_EXCEED_MAX_SIZE:
//				avServResetBuffer(hPC->spIdx,RESET_AUDIO,0);
				Log3("tutk av server send audio buffer full.");
				break;
			default:
				Log3("tutk av server send audio data failed.err:[%d].", ret);
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

	CPPPPChannel * hPC = (CPPPPChannel*)Ptr;
	if(hPC == NULL){
		Log2("Invalid channel class object.");
		return NULL;
	}

	long long   ts = 0;

	int nFrame = 0;
	int nBytesRead = 0;

	int firstKeyFrameComming = 0;
	int sts = time(NULL);
	int pts = 0;
	int fps = 0;
	int fix = 0;
//	int	isKeyFrame = 0;
	
	hPC->hAudioBuffer->Clear();
	hPC->hVideoBuffer->Clear();

	AV_HEAD * pVFrm = (AV_HEAD*)malloc(hPC->YUVSize/3);
	AV_HEAD * pAFrm = (AV_HEAD*)malloc(hPC->AudioSaveLength + sizeof(AV_HEAD));

	pAFrm->len = hPC->AudioSaveLength;

	while(hPC->recordingExit){

		int Type = WriteFrameType();

		if(Type < 0){
			usleep(10); continue;
		}
		
		if(Type){
			int vBytesHave = hPC->hVideoBuffer->Used();
			
			if(vBytesHave > (int)(sizeof(AV_HEAD))){
				nBytesRead = hPC->hVideoBuffer->Get((char*)pVFrm,sizeof(AV_HEAD));

				if(pVFrm->startcode != 0xa815aa55){
					Log3("invalid video frame lens:[%d].",pVFrm->len);
					hPC->hVideoBuffer->Clear();
					usleep(10); continue;
				}

				if(pVFrm->type == IPC_FRAME_FLAG_IFRAME){
					firstKeyFrameComming = 1;
				}

				if(firstKeyFrameComming != 1){
					hPC->hVideoBuffer->Mov(pVFrm->len);
					continue;
				}else{
					nBytesRead = hPC->hVideoBuffer->Get(pVFrm->d,pVFrm->len);
				}

				hPC->RecorderWrite(
					pVFrm->d,pVFrm->len,
					1,
					pVFrm->type == IPC_FRAME_FLAG_IFRAME ? 1: 0,
					ts);

				nFrame++;
			}else{
                
                if(pts <= 5) continue;
                if(fix == 0 || firstKeyFrameComming != 1){
                    continue;
                }
			
				Log3("recording fps:[%d] lost frame count:[%d] auto fix.\n",fps,fix);

				for(int i = 0;i < fix;i++){
					hPC->RecorderWrite(
						pVFrm->d,128,
						1,
						0,
						ts
						);
				}

				nFrame += fix;
			}

			pts = time(NULL) - sts;
			pts = pts > 0 ? pts : 1;
			
			fps = nFrame / pts;
			
			fix = hPC->FPS - fps;
			fix = fix > 0 ? fix : 0;
			
		}else{
            if(firstKeyFrameComming != 1){
                continue;
            }

#ifdef ENABLE_AUDIO_RECORD
			int aBytesHave = hPC->hAudioBuffer->Used();
			
			if(aBytesHave > pAFrm->len){
				nBytesRead = hPC->hAudioBuffer->Get(pAFrm->d,pAFrm->len);
				hPC->RecorderWrite(pAFrm->d,nBytesRead,0,0,ts);
			}else{
				memset(pAFrm->d,0,pAFrm->len);
				hPC->RecorderWrite(pAFrm->d,pAFrm->len,0,0,ts);
			}
#endif
		}
	}

	free(pAFrm); pAFrm = NULL;
	free(pVFrm); pVFrm = NULL;

	Log3("stop recording process done.");
	
	return NULL;
}

void * MeidaCoreProcess(
	void * hVoid
){
	SET_THREAD_NAME("MeidaCoreProcess");
	
	Log3("current thread id is:[%d].",gettid());

	CPPPPChannel * hPC = (CPPPPChannel*)hVoid;

	if(TRY_LOCK(&hPC->SessionLock) != 0){
		Log3("media core process still running.");
		return NULL;
	}

	JNIEnv * hEnv = NULL;
	
#ifdef PLATFORM_ANDROID
    char isAttached = 0;

	int err = g_JavaVM->GetEnv((void **) &(hEnv), JNI_VERSION_1_4);
	if(err < 0){
		err = g_JavaVM->AttachCurrentThread(&(hEnv), NULL);
		if(err < 0){
			Log3("iocmd send process AttachCurrentThread Failed!!");
			return NULL;
		}
		isAttached = 1;  
	}
#endif

	hPC->hCoreEnv = hEnv;

	int resend = 0;
    int status = 0;
	int Err = 0;

	hPC->startSession = 0;

connect:
	if(hPC->mediaLinking == 0){
		Log3("[0:%s]=====>close connection process by flag.",hPC->szDID);
		return NULL;
	}
	
    hPC->MsgNotify(hEnv, MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECTING);
	
	Log3("[1:%s]=====>start get free session id for client connection.",
         hPC->szDID);

	hPC->sessionID = IOTC_Get_SessionID();
	if(hPC->sessionID < 0){
		Log3("[1:%s]=====>IOTC_Get_SessionID error code [%d]\n",
             hPC->szDID,
             hPC->sessionID);
		goto jumperr;
	}

	Log3("[2:%s]=====>start connection by uid parallel.",hPC->szDID);
	
	hPC->SID = IOTC_Connect_ByUID_Parallel(hPC->szDID,hPC->sessionID);
	if(hPC->SID < 0){
		Log3("[2:%s]=====>start connection failed with error [%d] with device:[%s]\n",
             hPC->szDID, hPC->SID, hPC->szDID);
		
		switch(hPC->SID){
			case IOTC_ER_UNLICENSE:
                status = PPPP_STATUS_INVALID_ID;
				goto jumperr;
			case IOTC_ER_EXCEED_MAX_SESSION:
			case IOTC_ER_DEVICE_EXCEED_MAX_SESSION:
				status = PPPP_STATUS_EXCEED_SESSION;
				goto jumperr;
            case IOTC_ER_DEVICE_IS_SLEEP:
				Log3("[2:%s]=====>device in sleep mode.",hPC->szDID);
                status = PPPP_STATUS_DEVICE_SLEEP;
				if(TRY_LOCK(&hPC->PlayingLock) != 0){
					sleep(2);
					goto connect;
				}
				PUT_LOCK(&hPC->PlayingLock);
                goto jumperr;
            case IOTC_ER_CAN_NOT_FIND_DEVICE:
			case IOTC_ER_DEVICE_OFFLINE:
                Log3("[2:%s]=====>device not online,ask again.",hPC->szDID);
			case IOTC_ER_DEVICE_NOT_LISTENING:
				status = PPPP_STATUS_DEVICE_NOT_ON_LINE;
                IOTC_Session_Close(hPC->sessionID);
				goto jumperr;
			default:
				status = PPPP_STATUS_CONNECT_FAILED;
				goto jumperr;
		}
	}

	Log3("[3:%s]=====>start av client service with user:[%s] pass:[%s].\n", hPC->szDID, hPC->szUsr, hPC->szPwd);

	if(strlen(hPC->szUsr) == 0 || strlen(hPC->szPwd) == 0){
		status = PPPP_STATUS_NOT_LOGIN;
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
		0, 
		&resend
		);
	
	if(hPC->avIdx < 0){
		Log3("[3:%s]=====>avclient start failed:[%d] \n",
             hPC->szDID, hPC->avIdx);
		IOTC_Connect_Stop_BySID(hPC->SID);
		
		switch(hPC->avIdx){
			case AV_ER_INVALID_SID:
			case AV_ER_TIMEOUT:
			case AV_ER_REMOTE_TIMEOUT_DISCONNECT:
//				hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_CONNECT_TIMEOUT);
				break;
			case AV_ER_WRONG_VIEWACCorPWD:
			case AV_ER_NO_PERMISSION:
				status = PPPP_STATUS_INVALID_USER_PWD;
				goto jumperr;
			default:
				status = PPPP_STATUS_CONNECT_FAILED;
				goto jumperr;
		}
	
		sleep(1);
		goto connect;
	}

	Log3("[4:%s]=====>session:[%d] idx:[%d] did:[%s] resend:[%d].",
        hPC->szDID,
		hPC->SID,
		hPC->avIdx,
		hPC->szDID,
		resend
		);

	hPC->iocmdSending = 1;
    hPC->iocmdRecving = 1;
	hPC->audioPlaying = 1;
	hPC->videoPlaying = 1;
	
	hPC->audioEnabled = 1;
	hPC->voiceEnabled = 1;
	
	Err = pthread_create(&hPC->iocmdSendThread,NULL,IOCmdSendProcess,(void*)hPC);
	if(Err != 0){
		Log3("create iocmd send process failed.");
		goto jumperr;
	}
	
    Err = pthread_create(&hPC->iocmdRecvThread,NULL,IOCmdRecvProcess,(void*)hPC);
	if(Err != 0){
		Log3("create iocmd recv process failed.");
		goto jumperr;
	}

	Err = pthread_create(&hPC->audioRecvThread,NULL,AudioRecvProcess,(void*)hPC);
	if(Err != 0){
		Log3("create audio recv process failed.");
		goto jumperr;
	}

	Err = pthread_create(&hPC->audioSendThread,NULL,AudioSendProcess,(void*)hPC);
	if(Err != 0){
		Log3("create audio send process failed.");
		goto jumperr;
	}

	Err = pthread_create(&hPC->videoRecvThread,NULL,VideoRecvProcess,(void*)hPC);
	if(Err != 0){
		Log3("create video recv process failed.");
		goto jumperr;
	}

	Err = pthread_create(&hPC->videoPlayThread,NULL,VideoPlayProcess,(void*)hPC);
	if(Err != 0){
		Log3("create video play process failed.");
		goto jumperr;
	}

	hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_ON_LINE);

	while(hPC->mediaLinking){
		struct st_SInfo sInfo;
		int ret = IOTC_Session_Check(hPC->SID,&sInfo);
		if(ret < 0){
			hPC->mediaLinking = 0;
			Log3("IOTC_Session_Check failed with error:[%d]",ret);
			
			switch(ret){
				case IOTC_ER_DEVICE_OFFLINE:
					status = PPPP_STATUS_DEVICE_NOT_ON_LINE;
					break;
				case IOTC_ER_DEVICE_IS_SLEEP:
					status = PPPP_STATUS_DEVICE_SLEEP;
					break;
				default:
					if(hPC->connectionStatus != PPPP_STATUS_DEVICE_SLEEP){
						status = PPPP_STATUS_CONNECT_FAILED;
					}else{
						status = PPPP_STATUS_DEVICE_SLEEP;
					}
					break;
			}
			break;
		}

		if(hPC->startSession){	// for reconnect, we just refresh status for ui layer
			hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS, PPPP_STATUS_ON_LINE);
			hPC->startSession = 0;
		}
		
		sleep(5);
	}

jumperr:	
	
	GET_LOCK(&hPC->DestoryLock);
    
    hPC->PPPPClose();
    hPC->CloseWholeThreads(); // make sure other service thread all exit.
    hPC->MsgNotify(hEnv,MSG_NOTIFY_TYPE_PPPP_STATUS,status == 0 ? PPPP_STATUS_CONNECT_FAILED : status);

	PUT_LOCK(&hPC->DestoryLock);
	PUT_LOCK(&hPC->SessionLock);
    
    Log3("MediaCoreProcess Exit By Status:[%d].",status);

#ifdef PLATFORM_ANDROID
	if(isAttached) g_JavaVM->DetachCurrentThread();
#endif

	return NULL;	
}

CPPPPChannel::CPPPPChannel(
	char * did, 
	char * usr, 
	char * pwd,
	char * svr,
	char * connectionType
){ 
    memset(szDID, 0, sizeof(szDID));
    strcpy(szDID, did);

    memset(szUsr, 0, sizeof(szUsr));
    strcpy(szUsr, usr);

    memset(szPwd, 0, sizeof(szPwd));
    strcpy(szPwd, pwd);    

    memset(szSvr, 0, sizeof(szSvr));
    strcpy(szSvr, svr);
    
    iocmdRecving = 0;
    videoPlaying = 0;
	audioPlaying = 0;

	voiceEnabled = 0;
	audioEnabled = 0;
	speakEnabled = 1;
    
	mediaCoreThread = (pthread_t)-1;
	iocmdSendThread = (pthread_t)-1;
	iocmdRecvThread = (pthread_t)-1;
	videoPlayThread = (pthread_t)-1;
	videoRecvThread = (pthread_t)-1;
	audioSendThread = (pthread_t)-1;
	audioRecvThread = (pthread_t)-1;
	recordingThread = (pthread_t)-1;

	deviceType = -1;
	connectionStatus = -1;

	recordingExit = 0;
	avIdx = spIdx = rpIdx = sessionID = -1;

	startSession = 0;

    SID = -1;
    FPS = 25;

	hRecordFile = NULL;

	AudioSaveLength = 0;
	Audio10msLength = 0;

	MW = 1920;
	MH = 1080;
	
	MWCropSize = 0;
	MHCropSize = 0;
	
	YUVSize = (MW * MH) + (MW * MH)/2;

	hAudioBuffer = new CCircleBuffer( 128 * 1024);
	hSoundBuffer = new CCircleBuffer( 128 * 1024);
	hVideoBuffer = new CCircleBuffer(4096 * 1024);
	hIOCmdBuffer = new CCircleBuffer(COMMAND_BUFFER_SIZE);

	hAudioPutList = NULL;
	hAudioGetList = NULL;
	
//	hVideoBuffer->Debug(1);
    
    hDec = new CH264Decoder();

	INT_LOCK(&DisplayLock);
	INT_LOCK(&SessionLock);
	INT_LOCK(&CaptureLock);
	
	INT_LOCK(&PlayingLock);
	INT_LOCK(&DestoryLock);
	
}

CPPPPChannel::~CPPPPChannel()
{
    Log3("start free class pppp channel:[0] start.");
    Log3("start free class pppp channel:[1] close p2p connection and threads.");
    
    Close();  
    
    Log3("start free class pppp channel:[2] free buffer.");

//	hAudioBuffer->Release();
//	hVideoBuffer->Release();
//	hSoundBuffer->Release();
//	hIOCmdBuffer->Release();

	delete(hAudioBuffer);
	delete(hVideoBuffer);
	delete(hSoundBuffer);
	delete(hIOCmdBuffer);

//	hIOCmdBuffer = 
//	hAudioBuffer = 
//	hSoundBuffer = 
//	hVideoBuffer = NULL;
    
    Log3("start free class pppp channel:[3] free lock.");

	DEL_LOCK(&DisplayLock);
	DEL_LOCK(&CaptureLock);
	DEL_LOCK(&SessionLock);
	DEL_LOCK(&PlayingLock);
	DEL_LOCK(&DestoryLock);
    
    if(hDec) delete(hDec); hDec = NULL;
    
    Log3("start free class pppp channel:[4] close done.");
}

void CPPPPChannel::MsgNotify(
    JNIEnv * hEnv,
    int MsgType,
    int Param
){
    GET_LOCK( &g_CallbackContextLock );

	connectionStatus = Param;

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

	if(SID >= 0){
		avClientExit(SID,0);
		avServExit(SID,speakerChannel);	// for avServStart block
		IOTC_Session_Close(SID); 		// close client session handle
	}
	
	avIdx = spIdx = -1;
	SID = -1;
	sessionID = -1;

	return 0;
}

int CPPPPChannel::Start(char * usr,char * pwd,char * svr)
{   
	if(TRY_LOCK(&SessionLock) != 0){
		Log3("pppp connection with uuid:[%s] still running",szDID);
		startSession = 1;
		return -1;
	}

	memset(szUsr, 0, sizeof(szUsr));
    strcpy(szUsr, usr);

    memset(szPwd, 0, sizeof(szPwd));
    strcpy(szPwd, pwd);    

    memset(szSvr, 0, sizeof(szSvr));
    strcpy(szSvr, svr);

	mediaLinking = 1;

	Log3("start pppp connection to device with uuid:[%s].",szDID);
	pthread_create(&mediaCoreThread,NULL,MeidaCoreProcess,(void*)this);

	PUT_LOCK(&SessionLock);
	
    return 0;
}

void CPPPPChannel::Close()
{
	mediaLinking = 0;

	while(1){
		if(TRY_LOCK(&SessionLock) == 0){
			break;
		}
        
        IOTC_Connect_Stop_BySID(sessionID);
        avClientExit(SID,0);
		
        mediaLinking = 0;
		
		Log3("waiting for core media process exit.");
		sleep(1);
	}

	PUT_LOCK(&SessionLock);
}

int CPPPPChannel::SleepingStart(){	
	char avMsg[128] = {0};
	int  avErr = 0;
	
	SMsgAVIoctrlAVStream * pMsg = (SMsgAVIoctrlAVStream *)avMsg;

	avErr = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_DEVICESLEEP_REQ, 
		(char *)avMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(avErr < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",avErr,avIdx);
		return -1;
	}

	return 0;
}

int CPPPPChannel::SleepingClose(){
	IOTC_WakeUp_WakeDevice(szDID);
}

int CPPPPChannel::LiveplayStart(){
	char avMsg[1024] = {0};
	int  avErr = 0;

	if(szURL[0]){
		SMsgAVIoctrlPlayRecord * pMsg = (SMsgAVIoctrlPlayRecord *)avMsg;

		pMsg->command = AVIOCTRL_RECORD_PLAY_START;

		sscanf(szURL,"%d-%d-%d %d:%d:%d",
			&pMsg->stTimeDay.year,
			&pMsg->stTimeDay.month,
			&pMsg->stTimeDay.day,
			&pMsg->stTimeDay.hour,
			&pMsg->stTimeDay.minute,
			&pMsg->stTimeDay.second
		);
		
		pMsg->stTimeDay.wday = 0;

		Log3("start replay by url:[%s].",szURL);
		
		avErr = avSendIOCtrlEx(
			avIdx, 
			IOTYPE_USER_IPCAM_RECORD_PLAYCONTROL, 
			(char *)avMsg, 
			sizeof(SMsgAVIoctrlPlayRecord)
			);

		if(avErr != AV_ER_NoERROR){
			Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",avErr,SID,avIdx);
			return -1;
		}

		int times = 10;
		while(times-- && playrecChannel < 0){
			Log3("waiting for replay channel.");
			sleep(1);
		}

		if(playrecChannel < 0){
			Log3("get replay channel failed with error:[%d].",playrecChannel);
			return -1;
		}

		unsigned int svrType = 0;
		int svrRsnd = 1;

		rpIdx = avClientStart2(SID, szUsr, szPwd, 7, &svrType, playrecChannel, &svrRsnd);

		if(rpIdx < 0){
			Log3("avClientStart2 for replay failed:[%d].",rpIdx);
			return -1;
		}
	}else{
		avErr = avSendIOCtrlEx(
			avIdx, 
			IOTYPE_USER_IPCAM_START, 
			(char *)avMsg, 
			sizeof(SMsgAVIoctrlAVStream)
			);

		if(avErr != AV_ER_NoERROR){
			Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",avErr,SID,avIdx);
			return -1;
		}
	}
	
	return 0;
}

int CPPPPChannel::LiveplayClose(){
	char avMsg[1024] = {0};
	int  avErr = 0;

	if(szURL[0]){
		SMsgAVIoctrlPlayRecord * pMsg = (SMsgAVIoctrlPlayRecord *)avMsg;
		pMsg->command = AVIOCTRL_RECORD_PLAY_STOP;
		
		avErr = avSendIOCtrlEx(
			avIdx, 
			IOTYPE_USER_IPCAM_RECORD_PLAYCONTROL, 
			(char *)avMsg, 
			sizeof(SMsgAVIoctrlPlayRecord)
			);

		avClientStop(rpIdx); 
		rpIdx = -1;
		
	}else{
		avErr = avSendIOCtrlEx(
			avIdx, 
			IOTYPE_USER_IPCAM_STOP, 
			(char *)avMsg, 
			sizeof(SMsgAVIoctrlAVStream)
			);
	}

	if(avErr < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",avErr,avIdx);
		return -1;
	}

	return 0;
}

int CPPPPChannel::SpeakingStart(){
	
	char avMsg[128] = {0};
	SMsgAVIoctrlAVStream * pMsg = (SMsgAVIoctrlAVStream *)avMsg;
	
	int avErr = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_AUDIOSTART, 
		(char *)avMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(avErr != AV_ER_NoERROR){
		Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",avErr,SID,avIdx);
		return -1;
	}

	speakEnabled = 1;

	return 0;
}

int CPPPPChannel::SpeakingClose(){
	
	char avMsg[128] = {0};
	SMsgAVIoctrlAVStream * pMsg = (SMsgAVIoctrlAVStream *)avMsg;

	int avErr = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_AUDIOSTOP, 
		(char *)avMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(avErr < 0){
		Log3("avSendIOCtrl failed with err:[%d],avIdx:[%d].",avErr,avIdx);
		return -1;
	}

	speakEnabled = 0;

	return 0;
}

int CPPPPChannel::MicphoneStart(){
	if(spIdx >= 0){
		Log3("tutk audio send server already start.");
		return 0;
	}
	
	speakerChannel = IOTC_Session_Get_Free_Channel(SID);
	
    if(speakerChannel < 0){
        Log3("tutk get channel for audio send failed:[%d].",speakerChannel);
        return -1;
    }

	char avMsg[128] = {0};
	SMsgAVIoctrlAVStream * pMsg = (SMsgAVIoctrlAVStream *)avMsg;
    
	pMsg->channel = speakerChannel;

	int avErr = avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_SPEAKERSTART,
		(char *)avMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);
	
	if(avErr != AV_ER_NoERROR){
		Log3("avSendIOCtrl failed with err:[%d],sid:[%d],avIdx:[%d].",avErr,SID,avIdx);
		return -1;
	}
    
    Log3("tutk start audio send process by speaker channel:[%d].",speakerChannel);
    spIdx = avServStart(SID, NULL, NULL,  3, 0, speakerChannel);
    if(spIdx < 0){
		Log3("tutk start audio send server failed with error:[%d].",spIdx);
        return -1;
    }

	voiceEnabled = 1;

	return 0;
}

int CPPPPChannel::MicphoneClose(){
	char avMsg[128] = {0};
	SMsgAVIoctrlAVStream * pMsg = (SMsgAVIoctrlAVStream *)avMsg;
	
	pMsg->channel = speakerChannel;

	avSendIOCtrlEx(
		avIdx, 
		IOTYPE_USER_IPCAM_SPEAKERSTOP, 
		(char *)avMsg, 
		sizeof(SMsgAVIoctrlAVStream)
		);

	if(spIdx > 0) avServStop(spIdx);
	if(speakerChannel > 0) avServExit(SID,speakerChannel);
	
	voiceEnabled = 0;
	spIdx = -1;

	return 0;
}

int CPPPPChannel::CloseWholeThreads()
{
    //F_LOG;
	iocmdSending = 0;
    iocmdRecving = 0;
    audioPlaying = 0;
	videoPlaying = 0;
	
	recordingExit = 0;

	Log3("stop iocmd process.");
    if(iocmdSendThread != (pthread_t)-1) pthread_join(iocmdSendThread,NULL);
	if(iocmdRecvThread != (pthread_t)-1) pthread_join(iocmdRecvThread,NULL);
	iocmdRecvThread = (pthread_t)-1;

	Log3("stop video process.");
    if(videoRecvThread != (pthread_t)-1) pthread_join(videoRecvThread,NULL);
	if(videoPlayThread != (pthread_t)-1) pthread_join(videoPlayThread,NULL);

	Log3("stop audio process.");
    if(audioRecvThread != (pthread_t)-1) pthread_join(audioRecvThread,NULL);
  	if(audioSendThread != (pthread_t)-1) pthread_join(audioSendThread,NULL);

	Log3("stop recording process.");
	RecorderClose();

	iocmdRecvThread = (pthread_t)-1;
	iocmdSendThread = (pthread_t)-1;
	videoRecvThread = (pthread_t)-1;
	videoPlayThread = (pthread_t)-1;
	audioRecvThread = (pthread_t)-1;
	audioSendThread = (pthread_t)-1;

	Log3("stop media thread done.");

	return 0;

}

int CPPPPChannel::CloseMediaStreams(
){
	if(TRY_LOCK(&PlayingLock) == 0){
		Log3("stream not in playing");
		PUT_LOCK(&PlayingLock);
		return -1;
	}

	LiveplayClose(); // 关闭视频发送
	MicphoneClose(); // 关闭音频发送
	SpeakingClose(); // 关闭音频接收
	RecorderClose(); // 关闭本地录像
	
	SleepingStart(); // 打开设备休眠
	
	videoPlaying = 0;
	audioPlaying = 0;

	if(playrecChannel >= 0){
		Log3("close replay client.");
		avClientExit(SID,playrecChannel);
	}
    
	if(spIdx >= 0){
		Log3("close audio send server with idx:[%d]",spIdx);
		avServStop(spIdx);
	}
	
	if(speakerChannel >= 0){
		Log3("shutdown audio send server with sid:[%d] channel:[%d]",SID,speakerChannel);
		avServExit(SID,speakerChannel);
	}

	spIdx = -1;

	Log3("close media stream success ... ");

	PUT_LOCK(&PlayingLock);

	return 0;
}
	
int CPPPPChannel::StartMediaStreams(
	const char * url,
	int audio_sample_rate,
	int audio_channel,
	int audio_recv_codec,
	int audio_send_codec,
	int video_recv_codec,
	int video_w_crop,
	int video_h_crop
){    
    //F_LOG;	
	int ret = 0;
	int status = 0;
     
    if(SID < 0) return -1;

	if(TRY_LOCK(&PlayingLock) != 0){
		Log3("media stream already start. waiting for close");
		return -1;
	}

	if(TRY_LOCK(&DestoryLock) != 0){
		Log3("media stream will be destory.");
		PUT_LOCK(&PlayingLock);
		return -1;
	}

	while(1){
		GET_LOCK( &g_CallbackContextLock );
		status = connectionStatus;
		PUT_LOCK( &g_CallbackContextLock );

		if(status == PPPP_STATUS_ON_LINE){
			break;
		}else if(status == PPPP_STATUS_CONNECTING){
			Log3("media stream connecting, wait for a moment.");
			sleep(1); continue;
		}else{
			Log3("media stream already destory.");
			PUT_LOCK(&PlayingLock);
			PUT_LOCK(&DestoryLock);
			return -1;
		}
	}

	Log3("media stream start here.");

	// pppp://usr:pwd@livingstream:[channel id]
	// pppp://usr:pwd@replay/mnt/sdcard/replay/file
	memset(szURL,0,sizeof(szURL));

	AudioSampleRate = audio_sample_rate;
	AudioChannel = audio_channel;

	// only support channel mono, 16bit, 8KHz or 16KHz
	//   2 is come from 16bits/8bits = 2bytes
	// 100 is come from 10ms/1000ms
	Audio10msLength = audio_sample_rate * audio_channel * 2  / 100;
	AudioRecvFormat = audio_recv_codec;
	AudioSendFormat = audio_send_codec;
	VideoRecvFormat = video_recv_codec;

	MHCropSize = video_h_crop;
	MWCropSize = video_w_crop;

	videoPlaying = 1;
	audioPlaying = 1;

	Log3(
		"audio format info:[\n"
		"samplerate = %d\n"
		"channel = %d\n"
		"length in 10 ms is %d\n"
		"]\n",
		AudioSampleRate,AudioChannel,Audio10msLength);

	if(url != NULL){
		memcpy(szURL,url,strlen(url));
	}

    ret = LiveplayStart();
	
	PUT_LOCK(&DestoryLock);

    Log3("media stream start %s.",ret == 0 ? "done" : "fail");

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

	return hIOCmdBuffer->Put(AppCmds,sizeof(APP_CMD_HEAD) + hCmds->CgiLens);
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
		hEnv->DeleteLocalRef( resultDid  );
		hEnv->DeleteLocalRef( resultType );
		hEnv->DeleteLocalRef( resultTime );
	}
}

/*

*/

int CPPPPChannel::RecorderStart(
	int 		W,			// \BF\ED\B6\C8
	int 		H,			// \B8脽露\C8
	int 		FPS,		// 脰隆\C2\CA
	char *		SavePath	// 
){
	if(W == 0 || H == 0){
		W = this->MW;
		H = this->MH;
	}

	int Err = 0;

	if(FPS == 0){
		FPS = this->FPS;
    }else{
        this->FPS = FPS;
    }

	GET_LOCK(&CaptureLock);

	if(StartRecording(SavePath,FPS,W,H,this->AudioSampleRate,&AudioSaveLength) < 0){
		Log3("start recording with muxing failed.\n");
		goto jumperr;
	}

	recordingExit = 1;

	Err = pthread_create(
		&recordingThread,
		NULL,
		RecordingProcess,
		this);

	if(Err != 0){
		Log3("create av recording process failed.");
		CloseRecording();
		goto jumperr;
	}

	Log3("start recording process done.");

	PUT_LOCK(&CaptureLock);

	return  0;
	
jumperr:
	PUT_LOCK(&CaptureLock);

	return -1;
}

int CPPPPChannel::RecorderWrite(
	const char * 	FrameData,
	int				FrameSize,
	int				FrameCode, 	// audio or video codec [0|1]
	int				FrameType,	// keyframe or not [0|1]
	long long		Timestamp
){
//	Log3("frame write code and size:[%d][%d].\n",FrameCode,FrameSize);
	return WriteRecordingFrames((void*)FrameData,FrameSize,FrameCode,FrameType,Timestamp);
}

int CPPPPChannel::RecorderClose(){

	GET_LOCK(&CaptureLock);
	
	Log3("wait avi record process exit.");
	recordingExit = 0;
	if((long)recordingThread != -1){
		pthread_join(recordingThread,NULL);
		recordingThread = (pthread_t)-1;
	}
	Log3("avi record process exit done.");

	CloseRecording();

	PUT_LOCK(&CaptureLock);

	return 0;
}


