#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_codec.h"

#include "apprsp.h"
#include "appreq.h"
#include "echo_control_mobile.h"
#include "gain_control.h"
#include "noise_suppression_x.h"
#include "digital_agc.h"

#include "utility.h"

typedef struct{
	void *		hAgc;
	int 		MicLvI;
	int 		MicLvO;
}T_AGC_HANLE,*PT_AGC_HANDLE;

void * audio_nsx_init(
	int 	mode,
	int 	sampleRate
){
	NsxHandle * hNsx = WebRtcNsx_Create();
	
	if(hNsx == NULL){
		Log3("create webrtc nsx failed.");
		return NULL;
	}

	if(WebRtcNsx_Init(hNsx,sampleRate) != 0){
		Log3("failed in WebRtcNsx_set_policy.");
		WebRtcNsx_Free(hNsx);
		return NULL;
	}

	if(WebRtcNsx_set_policy(hNsx,mode) != 0){
		Log3("failed in WebRtcNsx_set_policy.");
		WebRtcNsx_Free(hNsx);
		return NULL;
	}

	return hNsx;
}

int    audio_nsx_proc(
	void * 	hNsx,
	char *	AudioBuffer,
	int		Audio10msLength
){
	if(!hNsx) return -1;

	int i = 0;

	short * I_FrmArray[1] = {0};
	short * O_FrmArray[1] = {0};
	short NsxData[160] = {0};

	I_FrmArray[0] = (short*)AudioBuffer;
	O_FrmArray[0] = (short*)NsxData;

	WebRtcNsx_Process((NsxHandle *)hNsx, I_FrmArray, 1, O_FrmArray);

	memcpy(AudioBuffer,NsxData,Audio10msLength);

	return 0;
}

void   audio_nsx_free(
	void *	hNsx
){
	if(hNsx) WebRtcNsx_Free((NsxHandle *)hNsx);
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
	agcConfig.limiterEnable = kAgcTrue;
	agcConfig.targetLevelDbfs = 3;
	
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
	int		Audio10msLength
){
	if(hAgc == NULL) return -1;

	PT_AGC_HANDLE Handle = (PT_AGC_HANDLE)hAgc;

	short * I_FrmArray[1] = {0};
	short * O_FrmArray[1] = {0};

	short AgcData[160] = {0};

	I_FrmArray[0] = (short*)AudioBuffer;
	O_FrmArray[0] = (short*)AgcData;
	
	int IMicLv = 0;
	int OMicLv = 0;
	
	int status = 0;

	unsigned char saturationWarning = 0;

	IMicLv = Handle->MicLvO;
	OMicLv = 0;

	status = WebRtcAgc_Process(Handle->hAgc,
		I_FrmArray,
		1,
		Audio10msLength / sizeof(short),
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

	memcpy(AudioBuffer,AgcData,Audio10msLength);

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

void * audio_vad_init(){
	AgcVad * hVad = (AgcVad*)malloc(sizeof(AgcVad));
	WebRtcAgc_InitVad(hVad);
	return hVad;
}

short  audio_vad_proc(
	void * hVad,
	char * AudioBuffer,
	int	   SampleCount
){
	return WebRtcAgc_ProcessVad((AgcVad *)hVad,(short *)AudioBuffer,SampleCount);
}

void audio_vad_free(
	void * hVad
){
	if(hVad) free(hVad);
	hVad = NULL;
}

void * audio_echo_cancellation_init(
	int echoMode,
	int sampleRate
){
	AecmConfig config;
	config.cngMode = true;
	config.echoMode = echoMode;

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


