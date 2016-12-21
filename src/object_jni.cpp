#include "stdio.h"
#include "utility.h"
#include <stdarg.h>
#include <dlfcn.h>

#include "object_jni.h"
#include "SearchDVS.h"
#include "PPPPChannelManagement.h"
#include "global_h264decoder.h"

#include "appreq.h"
#include "apprsp.h"

jobject   g_CallBack_Handle = NULL;
jmethodID g_CallBack_SearchResults = NULL;
jmethodID g_CallBack_VideoDataProcess = NULL;
jmethodID g_CallBack_AlarmNotifyDoorBell = NULL;
jmethodID g_CallBack_ConnectionNotify = NULL;
jmethodID g_Callback_UILayerNotify = NULL;

#ifdef PLATFORM_ANDROID
#include <jni.h>
JavaVM * g_JavaVM = NULL;
static const char * classPathName = ANDROID_CLASS_PATH;
#else
void * JNIEnv::GetByteArrayElements(char * a, int b){
    return a;
}

char * JNIEnv::GetStringUTFChars(char * a, int b){
    return a;
}

char * JNIEnv::NewStringUTF(char * a){
    char * r = NULL;
    if(a != NULL){
        if(strlen(a) > 0){
            r = (char*)malloc(strlen(a) + 1);
            memset(r,0,strlen(a) + 1);
            memcpy(r,a,strlen(a));
        }
    }
    return r;
}

void   JNIEnv::ReleaseByteArrayElements(char * a, char * b, int c){
    return;
}

void   JNIEnv::ReleaseStringUTFChars(char * a, char * b){
    return;
}

void   JNIEnv::DeleteLocalRef(char * a){
    if(a != NULL) free(a);
    return;
}

char * JNIEnv::NewByteArray(int a){
    return (char*)malloc(a);
}

void   JNIEnv::CallVoidMethod(void * a, void * b, ...){
    va_list args;
    va_start(args, b);
 
    /*
    if(b != NULL){
        void (*Call)(...) = (void (*)(...))b;
        Call(args);
        goto jumpout;
    }
    */

    if(b == g_CallBack_SearchResults){
        void (*Call)(int,const char *,const char *,const char *,const char *,int,int) = (void (*)(int,const char *,const char *,const char *,const char *,int,int))b;
        Call(va_arg(args,int),va_arg(args,const char *),va_arg(args,const char *),va_arg(args,const char *),va_arg(args,const char *),va_arg(args,int),va_arg(args,int));
        goto jumpout;
    }
    if(b == g_Callback_UILayerNotify){
        void (*Call)(const char *,int,const char *) = (void (*)(const char *,int,const char *))b;
        Call(va_arg(args,const char *),va_arg(args,int),va_arg(args,const char *));
        goto jumpout;
    }
    if(b == g_CallBack_VideoDataProcess){
        void (*Call)(const char *,char *,int,int,int,int,int) = (void (*)(const char *,char *,int,int,int,int,int))b;
        Call(va_arg(args,const char *),va_arg(args,char *),va_arg(args,int),va_arg(args,int),va_arg(args,int),va_arg(args,int),va_arg(args,int));
        goto jumpout;
    }
    if(b == g_CallBack_ConnectionNotify){
        void (*Call)(const char *,int,int) = (void (*)(const char *,int,int))b;
        Call(va_arg(args,const char *),va_arg(args,int),va_arg(args,int));
        goto jumpout;
    }
    if(b == g_CallBack_AlarmNotifyDoorBell){
        void (*Call)(const char *,const char *,const char *,const char *) = (void (*)(const char *,const char *,const char *,const char *))b;
        Call(va_arg(args,const char *),va_arg(args,const char *),va_arg(args,const char *),va_arg(args,const char *));
        goto jumpout;
    }
    
    Log3("Invalid callback function pointer.\n");
        
jumpout:
    va_end(args);
    
    return;
}

JNIEnv iOSEnv;

#endif

COMMO_LOCK g_CallbackContextLock = PTHREAD_MUTEX_INITIALIZER;
COMMO_LOCK g_FindDevsProcessLock = PTHREAD_MUTEX_INITIALIZER;

static CSearchDVS * g_PSearchDVS = NULL;
static CPPPPChannelManagement * g_pPPPPChannelMgt = NULL;

static void YUV4202RGB565_CALL(int width, int height, const unsigned char *src, unsigned short *dst)  
{  
    int line, col, linewidth;  
    int y, u, v, yy, vr, ug, vg, ub;  
    int r, g, b;  
    const unsigned char *py, *pu, *pv;  
      
    linewidth = width >> 1;  
    py = src;  
    pu = py + (width * height);  
    pv = pu + (width * height) / 4;  
      
    y = *py++;  
    yy = y << 8;  
    u = *pu - 128;  
    ug =  88 * u;  
    ub = 454 * u;  
    v = *pv - 128;  
    vg = 183 * v;  
    vr = 359 * v;  
      
    for (line = 0; line < height; line++) {  
       for (col = 0; col < width; col++) {  
        r = (yy +      vr) >> 8;  
        g = (yy - ug - vg) >> 8;  
        b = (yy + ub     ) >> 8;  
      
        if (r < 0)   r = 0;  
        if (r > 255) r = 255;  
        if (g < 0)   g = 0;  
        if (g > 255) g = 255;  
        if (b < 0)   b = 0;  
        if (b > 255) b = 255;  
       *dst++ = (((unsigned short)r>>3)<<11) | (((unsigned short)g>>2)<<5) | (((unsigned short)b>>3)<<0);  
        
        y = *py++;  
        yy = y << 8;  
        if (col & 1) {  
         pu++;  
         pv++;  
      
         u = *pu - 128;  
         ug =  88 * u;
         ub = 454 * u;  
         v = *pv - 128;  
         vg = 183 * v;  
         vr = 359 * v;  
        }  
       } /* ..for col */  
       if ((line & 1) == 0) { // even line: rewind  
        pu -= linewidth;  
        pv -= linewidth;  
       }  
    } /* ..for line */  
}  

JNIEXPORT int JNICALL YUV4202RGB565(JNIEnv *env, jobject obj, jbyteArray yuv, jbyteArray rgb, jint width, jint height)
{
    //F_LOG ;
    //Log("NativeCaller_YUV4202RGB565: width: %d, height: %d", width, height);

    jbyte * Buf = (jbyte*)env->GetByteArrayElements(yuv, 0);
    jbyte * Pixel= (jbyte*)env->GetByteArrayElements(rgb, 0);

    YUV4202RGB565_CALL(width, height, (const unsigned char *)Buf, (unsigned short *)Pixel);

    env->ReleaseByteArrayElements(yuv, Buf, 0);        
    env->ReleaseByteArrayElements(rgb, Pixel, 0);

    return 1;
}

JNIEXPORT void JNICALL PPPPInitialize(JNIEnv *env ,jobject obj, jstring svr)
{
	Log3("start pppp init with version:[%s.%s].",__DATE__,__TIME__);
	Log3("start pppp init with version:[%s.%s].",__DATE__,__TIME__);
	Log3("start pppp init with version:[%s.%s].",__DATE__,__TIME__);
}

JNIEXPORT void JNICALL PPPPManagementInit(JNIEnv *env ,jobject obj)
{   
    g_pPPPPChannelMgt = new CPPPPChannelManagement();

    global_init_decode();
}

JNIEXPORT void JNICALL PPPPManagementFree(JNIEnv *env ,jobject obj)
{
    SAFE_DELETE(g_pPPPPChannelMgt); 

    global_free_decoder();
}

JNIEXPORT int JNICALL PPPPSetCallbackContext(JNIEnv *env, jobject obj, jobject context)
{
    if(context == NULL){
       	Log3("set java callback for jni layer failed.\n");
		return -1;
    }else{
		GET_LOCK(&g_CallbackContextLock);

#ifdef PLATFORM_ANDROID
        g_CallBack_Handle = env->NewGlobalRef(context);
        jclass clazz = env->GetObjectClass(context);
		
        g_CallBack_SearchResults = env->GetMethodID(clazz, "SearchResult", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V") ;
		g_CallBack_AlarmNotifyDoorBell = env->GetMethodID( clazz, "CallBack_AlarmNotifyDoorBell", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V" );
        g_CallBack_VideoDataProcess = env->GetMethodID( clazz, "VideoData", "(Ljava/lang/String;[BIIIII)V" );

		g_CallBack_ConnectionNotify = env->GetMethodID(
			clazz, 
			"PPPPMsgNotify", 
			"(Ljava/lang/String;II)V"
			) ;

		// void UILayerNotify(String DeviceID,int MsgType,String MsgData)
		g_Callback_UILayerNotify = env->GetMethodID(
			clazz,
			"UILayerNotify",
			"(Ljava/lang/String;ILjava/lang/String;)V"
			);
#else
        g_CallBack_Handle = (jobject)malloc(sizeof(jobject));
        g_CallBack_SearchResults = (void*)CBSearchResults;
        g_Callback_UILayerNotify = (void*)CBUILayerNotify;
        g_CallBack_VideoDataProcess = (void*)CBVideoDataProcess;
        g_CallBack_ConnectionNotify = (void*)CBConnectionNotify;
        g_CallBack_AlarmNotifyDoorBell = (void*)CBAlarmNotifyByDevice;
#endif
		
		PUT_LOCK(&g_CallbackContextLock);
    } 

    return 0;
}

//ø™ ºÀ—À˜
JNIEXPORT void JNICALL StartSearch(JNIEnv *env ,jobject obj,jstring ssid,jstring psd)
{
    
    char *gkwifiname,*gkwifipwd; 
    gkwifiname  =   (char*)env->GetStringUTFChars(ssid,0);
    gkwifipwd   =   (char*)env->GetStringUTFChars(psd,0);
    
    GET_LOCK(&g_FindDevsProcessLock);
	
    SAFE_DELETE(g_PSearchDVS);
    g_PSearchDVS = new CSearchDVS();    
    g_PSearchDVS->Open(gkwifiname,gkwifipwd) ; 
    
    env->ReleaseStringUTFChars(ssid, gkwifiname);
    env->ReleaseStringUTFChars(psd, gkwifipwd);
	PUT_LOCK(&g_FindDevsProcessLock);
}

//Õ£÷πÀ—À˜
JNIEXPORT void JNICALL CloseSearch(JNIEnv *env ,jobject obj)
{
	GET_LOCK(&g_FindDevsProcessLock);

    SAFE_DELETE(g_PSearchDVS);

	PUT_LOCK(&g_FindDevsProcessLock);
}

JNIEXPORT int JNICALL StartPPPP(JNIEnv * env, jobject obj, jstring did, jstring user, jstring pwd, jstring server)
{
    //F_LOG;

	Log3("start pppp connection by native caller.");

    if(g_pPPPPChannelMgt == NULL){    
		Log3("channel management is null.\n");
        return 0;    
    }
    
    char *szDID,*szUsr,*szPwd,*szServer; 
    szDID    =   (char*)env->GetStringUTFChars(did,0);
    szUsr    =   (char*)env->GetStringUTFChars(user,0);
    szPwd    =   (char*)env->GetStringUTFChars(pwd,0);
    szServer =   (char*)env->GetStringUTFChars(server,0);

	Log3("user:%s pass:%s uuid:%s.\n",szUsr,szPwd,szDID);
	
    int nRet = g_pPPPPChannelMgt->Start(szDID, szUsr, szPwd, szServer);

    env->ReleaseStringUTFChars(pwd, szPwd);
    env->ReleaseStringUTFChars(user, szUsr);
    env->ReleaseStringUTFChars(did, szDID);
    env->ReleaseStringUTFChars(server, szServer);
	
    return nRet;

}

JNIEXPORT int JNICALL ClosePPPP(JNIEnv *env, jobject obj, jstring did)
{
    if(g_pPPPPChannelMgt == NULL){
        return 0;
    }
    
    char * szDID;
	
    szDID   =   (char*)env->GetStringUTFChars(did,0);
    if(szDID == NULL)
    {
        env->ReleaseStringUTFChars(did, szDID);
        return 0;
    }

	Log3("close pppp connection by native caller with uuid:[%s].",szDID);
    
    int nRet = g_pPPPPChannelMgt->Stop(szDID);

    env->ReleaseStringUTFChars(did, szDID);

    return nRet;

}

//Request for livestream
JNIEXPORT int JNICALL StartPPPPLivestream(
	JNIEnv *	env , 
	jobject 	obj ,
	jstring 	did , 				// p2p uuid
	jstring 	url ,				// url for record replay
	jint		audio_recv_codec,	// audio recv codec
	jint		audio_send_codec,	// audio send codec
	jint		video_recv_codec	// video recv codec
){
	int r = 0;
    
    if(g_pPPPPChannelMgt == NULL)
        return 0;

	char * szURL = NULL;
	char * szDID = NULL;

    szDID = (char*)env->GetStringUTFChars(did,0);
    if(szDID == NULL){        
        env->ReleaseStringUTFChars(did, szDID);
        goto jumperr;
    }

	szURL = (char*)env->GetStringUTFChars(url,0);
	if(szURL == NULL){
		goto jumperr;
	}

    Log3("click start live stream here: %s", szDID);
    
    r = g_pPPPPChannelMgt->StartPPPPLivestream(
		szDID, 
		szURL, 
		audio_recv_codec,
		audio_send_codec,
		video_recv_codec
		);

jumperr:
	
	if(szDID){
    	env->ReleaseStringUTFChars(did, szDID);    
	}
	
	if(szURL){
		env->ReleaseStringUTFChars(did, szURL);  
	}

    return r;
}

JNIEXPORT int JNICALL ClosePPPPLivestream(
	JNIEnv *	env, 
	jobject 	obj, 
	jstring 	did
){
    //F_LOG;
    
    if(g_pPPPPChannelMgt == NULL)
        return 0;

    char *szDID;
    szDID = (char*)env->GetStringUTFChars(did,0);
    if(szDID == NULL)
    {
        env->ReleaseStringUTFChars(did, szDID);
        return 0;
    }
    
    int nRet = g_pPPPPChannelMgt->ClosePPPPLivestream(szDID);
    env->ReleaseStringUTFChars(did, szDID);

    return nRet;
}

JNIEXPORT int JNICALL SetAudioStatus(JNIEnv *env , jobject obj, jstring did,jint status){
	char * szDID = (char*)env->GetStringUTFChars(did,0);

	int res = g_pPPPPChannelMgt->SetAudioStatus(szDID,(int)status);
	
	env->ReleaseStringUTFChars(did, szDID);

	return res;
}

JNIEXPORT int JNICALL GetAudioStatus(JNIEnv *env , jobject obj, jstring did){
	char * szDID = (char*)env->GetStringUTFChars(did,0);

	int res = g_pPPPPChannelMgt->GetAudioStatus(szDID);
	
	env->ReleaseStringUTFChars(did, szDID);

	return res;
}

JNIEXPORT int JNICALL SendHexsCommand(
	JNIEnv * 	env, 
	jobject 	obj, 
	jstring 	did, 
	jint 		msgtype, 
	jbyteArray 	msg,
	jint 		msglens
){
	char * szDID = (char*)env->GetStringUTFChars(did,0);
	jbyte * hexs = (jbyte*)env->GetByteArrayElements(msg,0);

	APP_BIN_DATA ABD;
	ABD.lens = msglens;
	memcpy(ABD.d,hexs,msglens);

	int MsgType = msgtype;
	int MsgLens = sizeof(ABD);

	g_pPPPPChannelMgt->PPPPSetSystemParams(
		szDID,MsgType,(char*)&ABD,MsgLens
		);

	env->ReleaseStringUTFChars(did,szDID);
	env->ReleaseByteArrayElements(msg,hexs,0); 

	return 0;
}

JNIEXPORT int JNICALL SendCtrlCommand(
	JNIEnv * 	env, 
	jobject 	obj, 
	jstring 	did, 
	jint 		msgtype, 
	jstring 	msg,
	jint 		msglens
){
	Log3("==========>SendCtrlCommand start with msg type:[%d]",msgtype);

	char * szDID = (char*)env->GetStringUTFChars(did,0);
	char * szMsg = (char*)env->GetStringUTFChars(msg,0);

	int MsgType = msgtype;
	int MsgLens = msglens;

	int Ret = g_pPPPPChannelMgt->PPPPSetSystemParams(
		szDID,MsgType,szMsg,MsgLens
		);

	env->ReleaseStringUTFChars(did,szDID);
	env->ReleaseStringUTFChars(msg,szMsg);

	Log3("==========>SendCtrlCommand close with msg type:[%d].",msgtype);

	return Ret;
}


JNIEXPORT int JNICALL StartRecorder(
	JNIEnv *env, jobject obj, jstring did, jstring filepath
){
	if(g_pPPPPChannelMgt == NULL) return 0;

	char * szDID 		= (char*)env->GetStringUTFChars(did,0);
	char * szSavePath = (char*)env->GetStringUTFChars(filepath,0);
	
	if(szDID == NULL){
		env->ReleaseStringUTFChars(did,szDID);
		return 0;
	}

	Log2("Nativecaller Call StartRecorder on DID:%s.",szDID);

	int ret = g_pPPPPChannelMgt->StartRecorderByDID(szDID,szSavePath);

	env->ReleaseStringUTFChars(did,szDID);
	env->ReleaseStringUTFChars(filepath,szSavePath);
	
	return ret == 0 ? 1 : 0;
}

JNIEXPORT int JNICALL CloseRecorder(
	JNIEnv *env, jobject obj, jstring did
){	
	if(g_pPPPPChannelMgt == NULL) return 0;

	char * szDID = (char*)env->GetStringUTFChars(did,0);
	if(szDID == NULL){
		env->ReleaseStringUTFChars(did,szDID);
		return 0;
	}

	Log2("Nativecaller Call CloseRecorder on DID:%s.",szDID);

	int ret = g_pPPPPChannelMgt->CloseRecorderByDID(szDID);

	env->ReleaseStringUTFChars(did,szDID);
	
	return ret == 0 ? 1 : 0;
}

#ifdef PLATFORM_ANDROID

static JNINativeMethod Calls[] = {
	{"YUV4202RGB565", "([B[BII)I", (void*)YUV4202RGB565},
	{"PPPPInitialize", "(Ljava/lang/String;)V", (void*)PPPPInitialize},
	{"StartSearch", "(Ljava/lang/String;Ljava/lang/String;)V", (void*)StartSearch},
	{"CloseSearch", "()V", (void*)CloseSearch},
	{"PPPPManagementInit", "()V", (void*)PPPPManagementInit},
	{"PPPPManagementFree", "()V", (void*)PPPPManagementFree},    
	{"PPPPSetCallbackContext", "(Landroid/content/Context;)I", (void*)PPPPSetCallbackContext},
	{"StartPPPP", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I", (void*)StartPPPP},
	{"ClosePPPP", "(Ljava/lang/String;)I", (void*)ClosePPPP},
	{"StartPPPPLivestream", "(Ljava/lang/String;Ljava/lang/String;III)I", (void*)StartPPPPLivestream},
	{"ClosePPPPLivestream", "(Ljava/lang/String;)I", (void*)ClosePPPPLivestream},
	{"StartRecorder", "(Ljava/lang/String;Ljava/lang/String;)I", (void*)StartRecorder},
	{"CloseRecorder", "(Ljava/lang/String;)I", (void*)CloseRecorder},
	{"SendHexsCommand", "(Ljava/lang/String;I[BI)I", (void*)SendHexsCommand},
	{"SendCtrlCommand", "(Ljava/lang/String;ILjava/lang/String;I)I", (void*)SendCtrlCommand},
	{"SetAudioStatus", "(Ljava/lang/String;I)I", (void*)SetAudioStatus},
	{"GetAudioStatus", "(Ljava/lang/String;)I", (void*)GetAudioStatus},
};

static int RegisterNativeMethods(JNIEnv* env, const char* className, JNINativeMethod * gMethods, int numMethods) {
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == NULL) {
		Log3("can't find class by name:[%s].\n",className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
		Log3("can't regist natives for class:[%s].\n",className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int RegisterNatives(JNIEnv* env){
    if (!RegisterNativeMethods(env, classPathName, Calls, sizeof(Calls) / sizeof(Calls[0]))){
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) 
{  
    g_JavaVM = vm ;

	JNIEnv* env = 0;
	jint result = -1;

	if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		goto bail;
	}

	if (!RegisterNatives(env)){
        goto bail;
    }
	
	result = JNI_VERSION_1_4;
	
    env->GetJavaVM(&g_JavaVM);   
bail:
	
	return result;
}

void JNI_OnUnload(JavaVM *vm, void *reserved)
{
   //F_LOG;
}

#endif


 
