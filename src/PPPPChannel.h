

#ifndef _PPPP_CHANNEL_H_
#define _PPPP_CHANNEL_H_

#include "CircleBuf.h"

#ifdef PLATFORM_ANDROID
#include <jni.h>
#include <sys/system_properties.h>
#include <sys/prctl.h>
#endif

#include <sys/resource.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

#include "H264Decoder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>

#include "ffmpeg_mp4.h"

#include "openxl_io.h"
#include "audio_codec.h"
#include "audiodatalist.h"

#include "libvdp.h"

#ifdef PLATFORM_ANDROID
#define SET_THREAD_NAME(name) \
{\
    prctl(PR_SET_NAME, (unsigned long)name, 0,0,0); \
    pid_t tid;\
    tid = gettid();\
}
#else
#define SET_THREAD_NAME(name)
#endif

#define MAX_PATHNAME_LEN 	256

//msgtype
#define MSG_NOTIFY_TYPE_PPPP_STATUS 0   	//
#define MSG_NOTIFY_TYPE_PPPP_MODE 1   		//
#define MSG_NOTIFY_TYPE_STREAM_TYPE 2

//pppp status
#define PPPP_STATUS_CONNECTING 0 			//
#define PPPP_STATUS_INITIALING 1 			///
#define PPPP_STATUS_ON_LINE 2 				//
#define PPPP_STATUS_CONNECT_FAILED 3		//
#define PPPP_STATUS_DISCONNECT 4 			//
#define PPPP_STATUS_INVALID_ID 5 			//
#define PPPP_STATUS_DEVICE_NOT_ON_LINE 6	//
#define PPPP_STATUS_CONNECT_TIMEOUT 7 		//
#define PPPP_STATUS_INVALID_USER_PWD 8 		//
#define PPPP_STATUS_DEVICE_SLEEP 9

#define PPPP_STATUS_NOT_LOGIN 11			//
#define PPPP_STATUS_EXCEED_SESSION 13		//

#define COMMAND_BUFFER_SIZE 32*1024

typedef struct tag_AV_HEAD
{
    unsigned int   		startcode;	//  0xa815aa55
    unsigned char		type;		//  0->264 idr frame 1->264 p frame
    unsigned char  	    streamid;	
    unsigned short  	militime;	//  diff time
    unsigned int 		sectime;	//  diff time
    unsigned int    	frameno;	//  frameno
    unsigned int 		len;		//  data len
    unsigned char		version;	// version
    unsigned char		sessid;		//ssid
    unsigned char		other[2];
    unsigned char		other1[8];
	char 				d[0];
}AV_HEAD,*PAV_HEAD;

//Command Channel head
typedef struct _CMD_CHANNEL_HEAD
{
    unsigned short startcode;
    unsigned short cmd;
    unsigned short len;
    unsigned short version;
	char 		   d[0];
}CMD_CHANNEL_HEAD, *PCMD_CHANNEL_HEAD;

typedef enum{
	STATUS_SESSION_START = 1,		
	STATUS_SESSION_CLOSE,		
	STATUS_SESSION_IDLE,
	STATUS_SESSION_DIED,
	STATUS_SESSION_START_PLAY,
	STATUS_SESSION_PLAYING,
	STATUS_SESSION_CLOSE_PLAY,
}SESSION_STATUS;

typedef double UINT64;

class CPPPPChannel
{
public:
    CPPPPChannel(char *DID, char *user, char *pwd,char *server);
    virtual ~CPPPPChannel();

    int  Start();
	void Close();

	int StartMediaChannel();
    int StartVideoChannel();
	int StartAudioChannel();
    int StartAlarmChannel();
    int StartIOCmdChannel();
	int StartIOSesChannel();
	
	int StartMediaStreams(
		const char * url,
		int audio_sample_rate,
		int audio_channel,
		int audio_recv_codec,
		int audio_send_codec,
		int video_recv_codec
		);
	
	int CloseMediaStreams();
	
	int CloseWholeThreads();
	
	int SendAVAPIStartIOCtrl();
	int SendAVAPICloseIOCtrl();
	int SetSystemParams(int type, char *msg, int len);
	
    void MsgNotify(JNIEnv * hEnv,int MsgType, int Param);

	void AlarmNotifyDoorBell(JNIEnv * hEnv, char *did, char *type, char *time );

	int  PPPPClose();

public:
	JNIEnv *            m_JNIMainEnv;	// Java env
	
	// 
	CAudioDataList *	hAudioGetList;
	CAudioDataList *	hAudioPutList;
	
	COMMO_LOCK			SessionStatusLock;
	int					SessionStatus;

	char 				szURL[256];		//

	char 				szDID[64];		//
    char 				szUsr[64];		//
    char 				szPwd[64]; 		//
    char 				szServer[1024];	//

	int					avstremChannel; // 
	int					speakerChannel;	//
	int					playrecChannel; //
	int					sessionID;		//
	int					avIdx;			//
	int					spIdx;			//
	unsigned int		deviceType;		//
    int                 deviceStandby;

	int					SID; 			// P2P session id
	
    int 				mediaEnabled;	// enable recv thread get media stream data.
	int					voiceEnabled;	// enable voice
	int					audioEnabled;	// enable audio
	int					speakEnabled;	// for vad detect we talk to device,set this to zero.

	int					mediaLinking;
	int 				videoPlaying;	
	int 				audioPlaying;		
	int 				iocmdRecving;	
	int					iocmdSending;

	int					AudioSampleRate;	// audio samplerate
	int					AudioChannel;		// audio channel mode
	int					AudioRecvFormat;	// audio codec from device
	int					AudioSendFormat;	// audio codec to device
	int					VideoRecvFormat;	// video codec from device

	int					Audio10msLength;	// audio data 10ms length

	pthread_t			mediaCoreThread;

	pthread_t 			iocmdRecvThread;
	pthread_t			iocmdSendThread;

	pthread_t 			videoPlayThread;
	pthread_t			videoRecvThread;

	pthread_t 			audioRecvThread;
	pthread_t 			audioSendThread;

	int					MW;				
	int					MH;				// 
	int					YUVSize;		//

    CH264Decoder    *   hDec;
	char 			* 	hVideoFrame;	//
	unsigned int		FrameTimestamp; //				;
	
	COMMO_LOCK			DisplayLock;	//
	COMMO_LOCK			SndplayLock;	//

	AVFormatContext * 	hAVFmtContext;
	AVOutputFormat  * 	hAVFmtOutput;
	AVStream		* 	hAudioStream;
	AVStream		* 	hVideoStream;
	AVCodecContext  * 	hAVCodecContext;

	OutputStream 		sVOs;
	OutputStream 		sAOs;

	char *				hAudioRecCaches;
	int					aLen;			// 


	long long		  	vCTS;			// 
	long long		  	vLTS;			// 
	long long		 	vPTS;			//

	long long			aCTS;			// 
	long long			aLTS;			// 
	long long		 	aPTS;			// 
	
	long long		  	sSTS;			// 

	long long			vIdx;			// 
	long long			aIdx;			// 

	char			*	hRecordFile;

	COMMO_LOCK			AviDataLock;
	PROCESS_HANDLE		avProc;			// 
	char 				avExit;			// 

	// for avi proc
	CCircleBuf *		hVideoBuffer;	// 
	CCircleBuf *		hAudioBuffer;	// 
	CCircleBuf *		hSoundBuffer;	// 
	
	CCircleBuf *		hIOCmdBuffer;	// 
	
	int StartRecorder(
		int 			W,				//
		int 			H,				// 
		int 			FPS,			// 
		char *			SavePath	
	);

	int WriteRecorder(
		const char * 	FrameData,
		int				FrameSize,
		int				FrameCode, 		// audio or video codec [0|1]
		int				FrameType,		// keyframe or not [0|1]
		long long		Timestamp
	);
	
	int CloseRecorder();
};

#endif
