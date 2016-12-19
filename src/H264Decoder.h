#ifndef _H264_DECODER_H_
#define _H264_DECODER_H_

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#ifdef __cplusplus
extern "C"{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

class CH264Decoder
{
public:
    CH264Decoder();
    ~CH264Decoder();

	void YUV4202RGB565(uint8_t *out);
    void YUV4202RGB565(uint8_t *yuv420, uint8_t *rgb565, int width, int height);
	int DecoderFrame(uint8_t *pbuf, int len,int &width ,int &height,int isKeyFrame);
    int GetYUVBuffer(uint8_t *pYUVBuffer, int bufLen);

private:
	void DeleteYUVTab();
	void CreateYUVTab_16();	
	void DisplayYUV_16(unsigned int *pdst1, unsigned char *y, unsigned char *u, unsigned char *v, int width, int height, int src_ystride, int src_uvstride, int dst_ystride);

private:
	AVCodecContext      *m_pCodecCtx;
    AVCodec             *m_pCodec;
    AVFrame             *m_pFrame;


	int 				*colortab ;
	int 				*u_b_tab ;
	int 				*u_g_tab ;
	int 				*v_g_tab ;
	int 				*v_r_tab ;

	unsigned int 		*rgb_2_pix ;
	unsigned int 		*r_2_pix ;
	unsigned int 		*g_2_pix ;
	unsigned int 		*b_2_pix ;
};

#endif
