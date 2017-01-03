#ifndef _AUDIO_CODEC_H_
#define _AUDIO_CODEC_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef pthread_mutex_t	COMMO_LOCK,*PCOMMO_LOCK;

#define GET_LOCK(X) 	pthread_mutex_lock(X)
#define PUT_LOCK(X) 	pthread_mutex_unlock(X)
#define TRY_LOCK(X) 	pthread_mutex_trylock(X)

#define INT_LOCK(X) 	pthread_mutex_init(X,NULL);
#define DEL_LOCK(X) 	pthread_mutex_destroy(X);

typedef enum{
    E_CODEC_AUDIO_AAC       = 0x88,   // 2014-07-02 add AAC audio codec definition
    E_CODEC_AUDIO_G711U     = 0x89,   //g711 u-law
    E_CODEC_AUDIO_G711A     = 0x8A,   //g711 a-law	
    E_CODEC_AUDIO_ADPCM     = 0X8B,
	E_CODEC_AUDIO_PCM		= 0x8C,
	E_CODEC_AUDIO_SPEEX		= 0x8D,
	E_CODEC_AUDIO_MP3		= 0x8E,
    E_CODEC_AUDIO_G726      = 0x8F,
	E_CODEC_AUDIO_OPUS		= 0xE1,
}E_AUDIO_CODEC;

void * audio_enc_init(
	int 	codec,
	int 	samplerate,
	int 	channel
);

void audio_enc_free(
	void * 	handle
);

int audio_enc_process(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
);

void * audio_dec_init(
	int 	codec,
	int 	samplerate,
	int 	channel
);

void audio_dec_free(
	void * 	handle
);

int audio_dec_process(
	void * 	handle,
	char *	enc,
	int  	enc_lens,
	char *  dec,
	int		dec_lens
);


void * audio_nsx_init(
	int 	mode,
	int 	sampleRate
);

int    audio_nsx_proc(
	void * 	hNsx,
	char *	AudioBuffer
);

void   audio_nsx_free(
	void *	hNsx
);

void * audio_agc_init(
	int 	gain,
	int 	mode,
	int 	minLv,
	int 	maxLv,
	int 	sampleRate
);

int audio_agc_proc(
	void * 	hAgc,
	char * 	AudioBuffer
);

void audio_agc_free(
	void * 	hAgc
);

void * audio_vad_init();

short  audio_vad_proc(
	void * hVad,
	char * AudioBuffer,
	int	   SampleCount
);

void audio_vad_free(
	void * hVad
);

void * audio_echo_cancellation_init(
	int echoMode,
	int sampleRate
);

int audio_echo_cancellation_farend(
	void * 		hAEC,
	char *		audioPlayingBuffer,
	int			frameSize
);

int audio_echo_cancellation_proc(
	void *		hAEC,
	char *		audioCaptureBuffer,
	char *		audioProcessBuffer,
	int			frameSize
);

void audio_echo_cancellation_free(
	void *		hAEC
);

#endif
