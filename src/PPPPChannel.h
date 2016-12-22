

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

#include "object_jni.h"

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
#define MSG_NOTIFY_TYPE_PPPP_STATUS 0   	/* ¡¨Ω”◊¥Ã¨ */
#define MSG_NOTIFY_TYPE_PPPP_MODE 1   		/* ¡¨Ω”ƒ£ Ω */
#define MSG_NOTIFY_TYPE_STREAM_TYPE 2

//pppp status
#define PPPP_STATUS_CONNECTING 0 			/* ’˝‘⁄¡¨Ω” */
#define PPPP_STATUS_INITIALING 1 			/* ’˝‘⁄≥ı ºªØ */
#define PPPP_STATUS_ON_LINE 2 				/* …Ë±∏‘⁄œﬂ */
#define PPPP_STATUS_CONNECT_FAILED 3		/* ¡¨Ω” ß∞‹ */
#define PPPP_STATUS_DISCONNECT 4 			/* ¡¨Ω”∂œø™ */
#define PPPP_STATUS_INVALID_ID 5 			/* Œﬁ–ß…Ë±∏ */
#define PPPP_STATUS_DEVICE_NOT_ON_LINE 6	/* …Ë±∏¿Îœﬂ */
#define PPPP_STATUS_CONNECT_TIMEOUT 7 		/* ¡¨Ω”≥¨ ± */
#define PPPP_STATUS_INVALID_USER_PWD 8 		/* Œﬁ–ß’Àªß√‹¬Î */

#define PPPP_STATUS_NOT_LOGIN 11			/* Œ¥µ«¬º */
#define PPPP_STATUS_EXCEED_SESSION 13		/* √ª”–ø…”√µƒª·ª∞ */

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

typedef struct{
	void *		hAgc;
	int 		MicLvI;
	int 		MicLvO;
}T_AGC_HANLE,*PT_AGC_HANDLE;

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
	
	// ”√”⁄ª∫¥Ê“—æ≠≤•∑≈π˝µƒ“Ù∆µ ˝æ›,”√“‘∂‘∆Î
	CAudioDataList *	hAudioGetList;
	CAudioDataList *	hAudioPutList;
	
    COMMO_LOCK			Lock;
	COMMO_LOCK			MediaReleaseLock;

	char 				szURL[256];		// ªÿ∑≈ ”∆µ¡¥Ω”

	char 				szDID[64];		// …Ë±∏ ID
    char 				szUsr[64];		// ”√ªß
    char 				szPwd[64]; 		// √‹¬Î
    char 				szServer[1024];	// ∑˛ŒÒ∆˜◊÷¥Æ

	int					avstremChannel; // 
	int					speakerChannel;	// ”Ô“Ù∑¢ÀÕÕ®µ¿
	int					sessionID;		// ª·ª∞ID
	int					avIdx;			// “Ù ”∆µÀ˜“˝
	int					spIdx;			// ”Ô“ÙÀ˜“˝
	unsigned int		deviceType;		// …Ë±∏¿‡–Õ

	int					SID; 			// P2P ª·ª∞
	
    int 				mediaEnabled;	// enable recv thread get media stream data.
	int					voiceEnabled;	// enable voice
	int					audioEnabled;	// enable audio
	int					speakEnabled;	// for vad detect we talk to device,set this to zero.

	int					mediaLinking;
	int 				videoPlaying;	
	int 				audioPlaying;		
	int 				iocmdRecving;	
	int					iocmdSending;

	int					AudioRecvFormat;
	int					VideoRecvFormat;
	int					AudioSendFormat;
	int					AudioEchoCancellationEnable;

	// ª·ª∞ºÏ≤‚œﬂ≥Ã
	pthread_t			mediaCoreThread;

	// ÷∏¡Ó¥¶¿Ìœﬂ≥Ã
	pthread_t 			iocmdRecvThread;
	pthread_t			iocmdSendThread;

	//  ”∆µ¥¶¿Ìœﬂ≥Ã
	pthread_t 			videoPlayThread;
	pthread_t			videoRecvThread;

	// “Ù∆µ¥¶¿Ìœﬂ≥Ã
	pthread_t 			audioRecvThread;
	pthread_t 			audioSendThread;

	int					MW;				// øÌ∂»
	int					MH;				// ∏ﬂ∂»
	int					YUVSize;		// YUV  ˝æ›¥Û–°

	char 			* 	hVideoFrame;	//  ”∆µ≤•∑≈ª∫≥Â«¯
	
	COMMO_LOCK			DisplayLock;	//  ”∆µ≤•∑≈À¯,”√”⁄±£’œ ”∆µª∫ªÊ÷∆≥Â«¯µƒ∂¿’º–‘(jbyte_yuv)
	COMMO_LOCK			SndplayLock;	// “Ù∆µ≤•∑≈À¯,”√”⁄±£’œ“Ù∆µª∫≤•∑≈≥Â«¯µƒ∂¿’º–‘(hAudioFrame)
	COMMO_LOCK			StartAVLock;	// “Ù ”∆µ≤•∑≈,Õ£÷π≤Ÿ◊˜À¯
	COMMO_LOCK			AVProcsLock;	// œﬂ≥Ãæ‰±˙À¯	

	AVFormatContext * 	hAVFmtContext;
	AVOutputFormat  * 	hAVFmtOutput;
	AVStream		* 	hAudioStream;
	AVStream		* 	hVideoStream;
	AVCodecContext  * 	hAVCodecContext;

	OutputStream 		sVOs;
	OutputStream 		sAOs;

	char *				hAudioRecCaches;
	int					aLen;			// 

	//  ”∆µ ±º‰¥¡

	long long		  	vCTS;			// µ±«∞ ±º‰¥¡
	long long		  	vLTS;			// …œ¥Œ ±º‰¥¡
	long long		 	vPTS;			// ø™ º ±º‰¥¡ - µ±«∞÷° ±º‰¥¡

	long long			aCTS;			// µ±«∞ ±º‰¥¡
	long long			aLTS;			// …œ¥Œ ±º‰¥¡
	long long		 	aPTS;			// ø™ º ±º‰¥¡ - µ±«∞÷° ±º‰¥¡
	
	long long		  	sSTS;			// ø™ º ±º‰¥¡


	// À˜“˝

	long long			vIdx;			//  ”∆µÀ˜“˝
	long long			aIdx;			// “Ù∆µÀ˜“˝

	char			*	hRecordFile;

	COMMO_LOCK			AviDataLock;
	PROCESS_HANDLE		avProc;			// ¬ºœÒœﬂ≥Ãæ‰±˙
	char 				avExit;			// ¬ºœÒœﬂ≥ÃÕÀ≥ˆ±Í÷æ

	// for avi proc
	CCircleBuf *		hVideoBuffer;	//  ”∆µª∫≥Â«¯
	CCircleBuf *		hAudioBuffer;	// “Ù∆µ≤•∑≈ª∫≥Â«¯
	CCircleBuf *		hSoundBuffer;	// “Ù∆µ¬º÷∆ª∫≥Â«¯
	
	CCircleBuf *		hIOCmdBuffer;	// ÷∏¡Ó∑¢ÀÕª∫≥Â«¯
	
	int StartRecorder(
		int 			W,				// øÌ∂»
		int 			H,				// ∏ﬂ∂»
		int 			FPS,			// ÷°¬ 
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
