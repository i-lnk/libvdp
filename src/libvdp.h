#ifndef _LIBVDP_H_
#define _LIBVDP_H_

// initialize order:
// 1. finish all callback on ios platform or java
// 2. call PPPPInitialize
// 3. call PPPPSetCallbackContext
// 4. call PPPPManagementInit
// 5. call StartPPPP to start p2p connection
// 7. call SendCtrlCommand to control device
// 8. call StartPPPPLivestream to start audio and video live stream
// 9. call ClosePPPPLivestream tp close audio and video live stream
// a. call ClosePPPP to close p2p connection

#ifdef PLATFORM_ANDROID

#include <jni.h>

#define ANDROID_CLASS_PATH  "com/edwintech/vdp/jni/Avapi" 	// for vdp
//#define ANDROID_CLASS_PATH  "com/rccar/jni/Avapi"			// for remote control toys

#else

#define JNIEXPORT
#define JNICALL

class JNIEnv{
public:
    void * GetByteArrayElements(char * a, int b);
    void   ReleaseByteArrayElements(char * a, char * b, int c);
    char * GetStringUTFChars(char * a, int b);
    void   ReleaseStringUTFChars(char * a, char * b);
    char * NewStringUTF(char * a);
    void   DeleteLocalRef(char * a);
    char * NewByteArray(int b);
    void   CallVoidMethod(void * a, void * b, ...);
};

typedef void * jobject;
typedef char * jbyteArray;
typedef char * jstring;
typedef  int   jint;
typedef void * jmethodID;
typedef char   jbyte;

// on iOS platform, you must pass this to all JNICALL functions.
// example:
// PPPPInitialize(&iOSEnv,NULL,NULL);
extern JNIEnv iOSEnv;

// callback for search device
// parameters:
// 1. unused, alaways be 1
// 2. device mac address
// 3. device name
// 4. device id, this is a important attribute for p2p connection
// 5. device ip
// 6. device port
// 7. device type
void CBSearchResults(int nTrue,const char * szMac,const char * szName,const char * szDID,const char * szIP,int nPort,int nType);

// callback for device response
// parameters:
// 1. device id,
// 2. cmd response for
// 3. response data in json format
void CBUILayerNotify(const char * szDID,int nCmd,const char * data,int datalens);

// callback for video YUV data
// parameters:
// 1. device id
// 2. yuv data
// 3. type
// 4. yuv data lens
// 5. width
// 6. height
// 7. timestamp for this frame
void CBVideoDataProcess(const char * szDID,char * lpImage,int nType,int nLens,int nW,int nH,int nTimestamp);

// callback when p2p connection status changed
// parameters:
// 1. device id
// 2. message type
// 3. message value
void CBConnectionNotify(const char * szDID,int nType,int nParam);

// callback when alarm message arrived
// parameters:
// 1. device id
// 2. device id 
// 3. message type,
// 4. message timestamp
void CBAlarmNotifyByDevice(const char * szDID,const char * szSID,const char * szType,const char * szTime);

#endif

#ifdef __cplusplus
extern "C" 
{
#endif
/*
* Class:     
* Method:    
* Signature: ()V14  
*/

JNIEXPORT int  JNICALL YUV4202RGB565(JNIEnv *, jobject, jbyteArray, jbyteArray, jint, jint);

// set android and ios callback function
// call this after PPPPInitialize
JNIEXPORT int  JNICALL PPPPSetCallbackContext(JNIEnv *, jobject, jobject);

// initialize p2p
JNIEXPORT void JNICALL PPPPInitialize(JNIEnv *, jobject, jstring);

// start device searching process
// parameters:
// 3. ssid
// 4. pass
JNIEXPORT void JNICALL StartSearch(JNIEnv *, jobject, jstring, jstring);

// stop device searching process
JNIEXPORT void JNICALL CloseSearch(JNIEnv *, jobject);

// initialize pppp management
JNIEXPORT void JNICALL PPPPManagementInit(JNIEnv *, jobject);

// free pppp management
JNIEXPORT void JNICALL PPPPManagementFree(JNIEnv *, jobject);

// start p2p connection by device id
// parameters:
// 3. device id
// 4. device username
// 5. device password
// 6. unused now

// return
// 0: success
JNIEXPORT int  JNICALL StartPPPP(JNIEnv *, jobject, jstring, jstring, jstring, jstring, jstring);

// close p2p connetcion by device id
// parameters:
// 3. device id

// return
// 0: success
JNIEXPORT int  JNICALL ClosePPPP(JNIEnv *, jobject, jstring);

// start get live stream by device id, yuv data send by callback function: CBVideoDataProcess

// parameters:
// 3. device id
// 4. url for replay
// 5. audio sample rate
// 6. audio channel
// 7. audio recv codec
// 8. audio send codec
// 9. unused now

// return
// 0: success
JNIEXPORT int  JNICALL StartPPPPLivestream(JNIEnv *, jobject, jstring, jstring, jint, jint, jint, jint, jint);


// close live stream by device id
// parameters:
// 3. device id

// return
// 0: success
JNIEXPORT int  JNICALL ClosePPPPLivestream(JNIEnv *, jobject, jstring);

// start live stream recoder by device id
// parameters:
// 3. device id
// 4. filepath for mp4 (fullpath)

// return
// 0: success
JNIEXPORT int  JNICALL StartRecorder(JNIEnv *, jobject, jstring, jstring);

// close live stream recoder by device id

// return
// 0: success
JNIEXPORT int  JNICALL CloseRecorder(JNIEnv *, jobject, jstring);

// for serial port data relay
// parameters:
// 3. device id
// 5. msg data
// 6. msg data lens

// return
// 0: success
JNIEXPORT int  JNICALL SendBytes(JNIEnv *, jobject, jstring, jbyteArray, jint);

// send device control command
// parameters:
// 3. device id
// 4. msg type
// 5. msg data
// 6. msg data lens 

// control msg format:
// just like http get request.
// xxxx.cgi?param1=base64[val]&param2=base64[val]

// return
// 0: success
JNIEXPORT int  JNICALL SendCtrlCommand(JNIEnv *, jobject, jstring, jint, jstring, jint);

// set current audio status:
// parameters:
// 3. device id
// 4. value (bit:0 audio play enable, bit1:audio capture enable)

// return value: (bit:0 audio play enable, bit1:audio capture enable)
JNIEXPORT int  JNICALL SetAudioStatus(JNIEnv *, jobject, jstring, jint);

// get current audio status:
// parameters:
// 3. device id

// return value: (bit:0 audio play enable, bit1:audio capture enable)
JNIEXPORT int  JNICALL GetAudioStatus(JNIEnv *, jobject, jstring);

#ifdef __cplusplus
}
#endif

#endif


