#include "audio_codec_adpcm.h"

static int gs_index_adjust[8]= {-1,-1,-1,-1,2,4,6,8};
static int gs_step_table[89] = 
{
	7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
	50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,
	408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,
	2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
	10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};

static int m_nEnAudioPreSample;
static int m_nEnAudioIndex;
static int m_nDeAudioPreSample;
static int m_nDeAudioIndex;

void audio_adpcm_enc_init(){
    m_nEnAudioPreSample = 0;
    m_nEnAudioIndex = 0;
    m_nDeAudioPreSample = 0;
    m_nDeAudioIndex = 0;
}

static void audio_adpcm_enc_clr(){
	m_nEnAudioPreSample = 0;
	m_nEnAudioIndex = 0;
}

static void audio_adpcm_dec_clr(){
	m_nDeAudioPreSample = 0;
	m_nDeAudioIndex = 0;
}

void audio_adpcm_enc(unsigned char *pRaw, int nLenRaw, unsigned char *pBufEncoded)
{
	short *pcm = (short *)pRaw;
	int cur_sample;
	int i;
	int delta;
	int sb;
	int code;
	nLenRaw >>= 1;

    audio_adpcm_enc_clr();

	for(i = 0; i<nLenRaw; i++)
	{
		cur_sample = pcm[i]; 
		delta = cur_sample - m_nEnAudioPreSample;
		if (delta < 0){
			delta = -delta;
			sb = 8;
		}else sb = 0;

		code = 4 * delta / gs_step_table[m_nEnAudioIndex];	
		if (code>7)	code=7;

		delta = (gs_step_table[m_nEnAudioIndex] * code) / 4 + gs_step_table[m_nEnAudioIndex] / 8;
		if(sb) delta = -delta;

		m_nEnAudioPreSample += delta;
		if (m_nEnAudioPreSample > 32767) m_nEnAudioPreSample = 32767;
		else if (m_nEnAudioPreSample < -32768) m_nEnAudioPreSample = -32768;

		m_nEnAudioIndex += gs_index_adjust[code];
		if(m_nEnAudioIndex < 0) m_nEnAudioIndex = 0;
		else if(m_nEnAudioIndex > 88) m_nEnAudioIndex = 88;

		if(i & 0x01) pBufEncoded[i>>1] |= code | sb;
		else pBufEncoded[i>>1] = (code | sb) << 4;
	}
}

void audio_adpcm_dec(char *pDataCompressed, int nLenData, char *pDecoded)
{
	int i;
	int code;
	int sb;
	int delta;
	short *pcm = (short *)pDecoded;
	nLenData <<= 1;

    audio_adpcm_dec_clr();

	for(i=0; i<nLenData; i++)
	{
		if(i & 0x01) code = pDataCompressed[i>>1] & 0x0f;
		else code = pDataCompressed[i>>1] >> 4;

		if((code & 8) != 0) sb = 1;
		else sb = 0;
		code &= 7;

		delta = (gs_step_table[m_nDeAudioIndex] * code) / 4 + gs_step_table[m_nDeAudioIndex] / 8;
		if(sb) delta = -delta;

		m_nDeAudioPreSample += delta;
		if(m_nDeAudioPreSample > 32767) m_nDeAudioPreSample = 32767;
		else if (m_nDeAudioPreSample < -32768) m_nDeAudioPreSample = -32768;

		pcm[i] = m_nDeAudioPreSample;
		m_nDeAudioIndex+= gs_index_adjust[code];
		if(m_nDeAudioIndex < 0) m_nDeAudioIndex = 0;
		if(m_nDeAudioIndex > 88) m_nDeAudioIndex= 88;
	}

	return;
}
