#ifndef _ADPCM_H_
#define _ADPCM_H_

void audio_adpcm_enc_init();
void audio_adpcm_enc(unsigned char * pRaw, int nLenRaw, unsigned char * pBufEncoded);
void audio_adpcm_dec(char * pDataCompressed, int nLenData, char * pDecoded);

#endif

