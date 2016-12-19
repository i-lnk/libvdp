
#ifndef _GLOBAL_H264_DECODER_H_
#define _GLOBAL_H264_DECODER_H_

void global_init_decode();
void global_free_decoder();
int global_decode_one_frame(char*pbuf, int len, char **y, char** u, char **v, int *width, int *height);
int global_decode_one_frame_with_buf(char*pbuf, int len, char *yuv, int *width, int *height);


#endif


