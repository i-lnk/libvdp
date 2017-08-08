#ifndef _AUDIO_CODEC_G711_H_
#define _AUDIO_CODEC_G711_H_

unsigned int audio_alaw_enc(unsigned char *dst, short *src, unsigned int size);
unsigned int audio_alaw_dec(short *dst, const unsigned char *src, unsigned int size);

#endif
