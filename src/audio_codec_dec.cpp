#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_codec.h"
#include <pthread.h>
#include "opus.h"
#include "audio_codec_adpcm.h"
#include "audio_codec_g711.h"

static const int audio_eb_size = 8192;

typedef struct{
	int codec;
	int samplerate;
	int channel;

	void *	hdec;

	COMMO_LOCK * plock;
}T_AUDIO_CODEC,*PT_AUDIO_CODEC;

static int audio_dec_g711(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	return audio_alaw_dec((short *)out,(const unsigned char *)raw,raw_lens);
}

static int audio_dec_opus(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;
	OpusDecoder * hOpusDec = (OpusDecoder *)hCodec->hdec;
	int declens = opus_decode(hOpusDec,(const unsigned char *)raw,raw_lens,(opus_int16 *)out,out_lens,0);
	return declens > 0 ? declens * 2 : declens;
}

static int audio_dec_pcm(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	int lens = out_lens > raw_lens ? raw_lens : out_lens;
	memcpy(out,raw,lens);
	return lens;
}

static int audio_dec_adpcm(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
    return -1;
}

void * audio_dec_init(
	int codec,
	int samplerate,
	int channel
){
	PT_AUDIO_CODEC hCodec = NULL;
	
	int ret = 0;
	OpusDecoder * hOpusDec = NULL;

	switch(codec){
		case E_CODEC_AUDIO_G711A:
			hCodec = (PT_AUDIO_CODEC)malloc(sizeof(T_AUDIO_CODEC));
			break;
		case E_CODEC_AUDIO_OPUS:
			hOpusDec = opus_decoder_create(samplerate,channel,&ret);
			if(ret != OPUS_OK){
				return NULL;
			}
			
			hCodec = (PT_AUDIO_CODEC)malloc(sizeof(T_AUDIO_CODEC));
			hCodec->hdec = hOpusDec;
			break;
		case E_CODEC_AUDIO_PCM:
			hCodec = (PT_AUDIO_CODEC)malloc(sizeof(T_AUDIO_CODEC));
			break;
		case E_CODEC_AUDIO_ADPCM:
			return NULL;
		default:
			return NULL;
	}

	hCodec->channel = channel;
	hCodec->samplerate = samplerate;
	hCodec->codec = codec;

	hCodec->plock = (COMMO_LOCK*)malloc(sizeof(COMMO_LOCK));

	INT_LOCK(hCodec->plock);

	return hCodec;
}

void audio_dec_free(
	void * 	handle
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;

	if(hCodec == NULL) return;

	COMMO_LOCK * plock = hCodec->plock;

	GET_LOCK(plock);

	switch(hCodec->codec){
		case E_CODEC_AUDIO_G711A:
			break;
		case E_CODEC_AUDIO_OPUS:
			if(hCodec->hdec != NULL){
				opus_decoder_destroy((OpusDecoder *)hCodec->hdec);
			}
			break;
		case E_CODEC_AUDIO_PCM:
			break;
		case E_CODEC_AUDIO_ADPCM:
			break;
	}

	free(hCodec);
	PUT_LOCK(plock);

	DEL_LOCK(plock);
	free(plock);
	plock = NULL;
}

int audio_dec_process(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;

	if(hCodec == NULL){
		return -1;
	}

	GET_LOCK(hCodec->plock);

	int ret = -1;
	
	switch(hCodec->codec){
		case E_CODEC_AUDIO_G711A:
			ret = audio_dec_g711(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_OPUS:
			ret = audio_dec_opus(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_PCM:
			ret = audio_dec_pcm(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_ADPCM:
			ret = audio_dec_adpcm(handle,raw,raw_lens,out,out_lens);
			break;
	}

	PUT_LOCK(hCodec->plock);

	return ret;
}
