#ifndef _PPPP_CHANNEL_MANAGEMENT_H_
#define _PPPP_CHANNEL_MANAGEMENT_H_

#include "PPPPChannel.h"

#define MAX_DID_LENGTH 64
#define MAX_PPPP_CHANNEL_NUM 64

typedef struct T_SESSION{
    char 			deviceID[MAX_DID_LENGTH] ;
    CPPPPChannel * 	session;
   	COMMO_LOCK 		lock;
}T_SESSION, *PT_SESSION;

class CPPPPChannelManagement
{
public:
    CPPPPChannelManagement();
    ~CPPPPChannelManagement();

    int Start(char *szDID, char *user, char *pwd,char *server,char *connectionType);
    int Close(char *szDID);
    void CloseAll();

    int StartPPPPLivestream(
		char * szDID, 
		char * szURL,
		int audio_sample_rate,
		int	audio_channel,
		int audio_recv_codec,
		int audio_send_codec,
		int video_recv_codec,
		int video_w_crop,
		int video_h_crop
		);
	
    int ClosePPPPLivestream(char *szDID);

	int GetAudioStatus(char * szDID);
	int SetAudioStatus(char * szDID,int AudioStatus);
	int SetVideoStatus(char * szDID,int VideoStatus);

    int PPPPIOCmdSend(char *szDID, int type, char *msg, int len);

	int StartRecorderByDID(char * szDID,char * filepath);
	int CloseRecorderByDID(char * szDID);
	
private:
    T_SESSION sessionList[MAX_PPPP_CHANNEL_NUM];
};

#endif
