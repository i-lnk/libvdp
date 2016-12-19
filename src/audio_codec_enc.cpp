#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_codec.h"
#include "opus.h"
#include "audio_codec_adpcm.h"
#include "audio_codec_g711.h"

//static const int audio_eb_size = 8192;

typedef struct{
	int codec;
	int samplerate;
	int channel;
	
	void *	henc;

	COMMO_LOCK * plock;
}T_AUDIO_CODEC,*PT_AUDIO_CODEC;

static int audio_enc_g711(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	return audio_alaw_enc((unsigned char *)out,(short *)raw,raw_lens);
}

static int audio_enc_opus(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;
	OpusEncoder * hOpusEnc = (OpusEncoder *)hCodec->henc;
	return opus_encode(hOpusEnc,(const opus_int16 *)raw,raw_lens/2,(unsigned char *)out,out_lens);
}

static int audio_enc_pcm(
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

static int audio_enc_adpcm(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
    return -1;
}

void * audio_enc_init(
	int codec,
	int samplerate,
	int channel
){
	PT_AUDIO_CODEC hCodec = NULL;
	int ret = 0;

	OpusEncoder * hOpusEnc = NULL;

	switch(codec){
		case E_CODEC_AUDIO_G711A:
			hCodec = (PT_AUDIO_CODEC)malloc(sizeof(T_AUDIO_CODEC));
			break;
		case E_CODEC_AUDIO_OPUS:
			hOpusEnc = opus_encoder_create(samplerate, channel, OPUS_APPLICATION_VOIP, &ret);
			if(ret != OPUS_OK){
				return NULL;
			}

			opus_encoder_ctl(hOpusEnc, OPUS_SET_BITRATE(16000));
//			opus_encoder_ctl(hOpusEnc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
			opus_encoder_ctl(hOpusEnc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)); 
    		opus_encoder_ctl(hOpusEnc, OPUS_SET_COMPLEXITY(4));  
    		opus_encoder_ctl(hOpusEnc, OPUS_SET_INBAND_FEC(0)); 
//  		opus_encoder_ctl(hOpusEnc, OPUS_SET_DTX(0));  
//  		opus_encoder_ctl(hOpusEnc, OPUS_SET_PACKET_LOSS_PERC(0));  
//  		opus_encoder_ctl(hOpusEnc, OPUS_GET_LOOKAHEAD(&skip));  
//  		opus_encoder_ctl(hOpusEnc, OPUS_SET_LSB_DEPTH(16));  
//			opus_encoder_ctl(hOpusEnc, OPUS_SET_FORCE_CHANNELS(1));

			hCodec = (PT_AUDIO_CODEC)malloc(sizeof(T_AUDIO_CODEC));
			hCodec->henc = hOpusEnc;
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

void audio_enc_free(
	void * 	handle
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;
	if(hCodec == NULL) return;

	COMMO_LOCK * plock = hCodec->plock;

	GET_LOCK(plock);

	switch(hCodec->codec){
		case E_CODEC_AUDIO_G711A:
		case E_CODEC_AUDIO_G711U:
			break;
		case E_CODEC_AUDIO_OPUS:
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

int audio_enc_process(
	void * 	handle,
	char *	raw,
	int  	raw_lens,
	char *  out,
	int		out_lens
){
	PT_AUDIO_CODEC hCodec = (PT_AUDIO_CODEC)handle;

	GET_LOCK(hCodec->plock);

	if(hCodec == NULL){
		PUT_LOCK(hCodec->plock);
		return -1;
	}
	
	int ret = -1;
	
	switch(hCodec->codec){
		case E_CODEC_AUDIO_G711A:
			ret = audio_enc_g711(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_OPUS:
			ret = audio_enc_opus(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_PCM:
			ret = audio_enc_pcm(handle,raw,raw_lens,out,out_lens);
			break;
		case E_CODEC_AUDIO_ADPCM:
			ret = audio_enc_adpcm(handle,raw,raw_lens,out,out_lens);
			break;
	}
	
	PUT_LOCK(hCodec->plock);

	return ret;
}

