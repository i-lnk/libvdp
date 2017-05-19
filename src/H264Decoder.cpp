#ifdef PLATFORM_ANDROID
#include <jni.h>
#include <dlfcn.h>
extern int g_sdkVersion;
#else
#include <CoreMedia/CoreMedia.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "H264Decoder.h"
#include "utility.h"

CH264Decoder::CH264Decoder()
{
    //F_LOG ;   

#ifdef PLATFORM_ANDROID
#else
    /*
    const unsigned char * parameterSetPointers[2] = {sps,pps};
    const size_t parameterSetSizes[2] = {spsSize,ppsSize};
    
    CMFormatDescriptionRef fmt;
    CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,2,parameterSetPointers,parameterSetSizes,4,&fmt);
    */
#endif

    //==========================================
    colortab = NULL;
    u_b_tab = NULL;
    u_g_tab = NULL;
    v_g_tab = NULL;
    v_r_tab = NULL;

    rgb_2_pix = NULL;
    r_2_pix = NULL;
    g_2_pix = NULL;
    b_2_pix = NULL;

    //===========================================

    CreateYUVTab_16();

    av_register_all();      

    m_pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if(m_pCodec == NULL)
    {
        Log("pCodec == NULL\n");
        return ;
    }

    m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
    if(m_pCodecCtx == NULL)
    {
        Log("if(pCodecCtx == NULL)\n");
        return ;
    }  
  
    //¥Úø™codec°£»Áπ˚¥Úø™≥…π¶µƒª∞£¨∑÷≈‰AVFrame£     
    if(avcodec_open2(m_pCodecCtx, m_pCodec, NULL) >= 0)     
    {     
//        m_pFrame = avcodec_alloc_frame();   /* Allocate video frame   */
		  m_pFrame = av_frame_alloc();        
    }
}

void CH264Decoder::DeleteYUVTab()
{	
    av_free(colortab);
	av_free(rgb_2_pix);
    
    colortab = NULL;
    rgb_2_pix = NULL;
}

void CH264Decoder::CreateYUVTab_16()
{
	int i;
	int u, v;
	
	colortab = (int *)av_malloc(4*256*sizeof(int));
	u_b_tab = &colortab[0*256];
	u_g_tab = &colortab[1*256];
	v_g_tab = &colortab[2*256];
	v_r_tab = &colortab[3*256];

	for (i=0; i<256; i++)
	{
		u = v = (i-128);

		u_b_tab[i] = (int) ( 1.772 * u);
		u_g_tab[i] = (int) ( 0.34414 * u);
		v_g_tab[i] = (int) ( 0.71414 * v); 
		v_r_tab[i] = (int) ( 1.402 * v);
	}

	rgb_2_pix = (unsigned int *)av_malloc(3*768*sizeof(unsigned int));

	r_2_pix = &rgb_2_pix[0*768];
	g_2_pix = &rgb_2_pix[1*768];
	b_2_pix = &rgb_2_pix[2*768];

	for(i=0; i<256; i++)
	{
		r_2_pix[i] = 0;
		g_2_pix[i] = 0;
		b_2_pix[i] = 0;
	}

	for(i=0; i<256; i++)
	{
		r_2_pix[i+256] = (i & 0xF8) << 8;
		g_2_pix[i+256] = (i & 0xFC) << 3;
		b_2_pix[i+256] = (i ) >> 3;
	}

	for(i=0; i<256; i++)
	{
		r_2_pix[i+512] = 0xF8 << 8;
		g_2_pix[i+512] = 0xFC << 3;
		b_2_pix[i+512] = 0x1F;
	}

	r_2_pix += 256;
	g_2_pix += 256;
	b_2_pix += 256;
}

void CH264Decoder::DisplayYUV_16(unsigned int *pdst1, 
                                            unsigned char *y, 
                                            unsigned char *u, 
                                            unsigned char *v, 
                                            int width, 
                                            int height, 
                                            int src_ystride, 
                                            int src_uvstride, 
                                            int dst_ystride)
{
	int i, j;
	int r, g, b, rgb;

	int yy, ub, ug, vg, vr;

	unsigned char* yoff;
	unsigned char* uoff;
	unsigned char* voff;
	
	unsigned int* pdst=pdst1;

	int width2 = width/2;
	int height2 = height/2;

    #if 0
	if(width2>width/2)
	{
		width2=width/2;

		y+=(width-width)/4*2;
		u+=(width-width)/4;
		v+=(width-width)/4;
	}

	if(height2>height)
		height2=height;

    #endif

	for(j=0; j<height2; j++) // “ª¥Œ2x2π≤Àƒ∏ˆœÒÀÿ
	{
		yoff = y + j * 2 * src_ystride;
		uoff = u + j * src_uvstride;
		voff = v + j * src_uvstride;

		for(i=0; i<width2; i++)
		{
			yy  = *(yoff+(i<<1));
			ub = u_b_tab[*(uoff+i)];
			ug = u_g_tab[*(uoff+i)];
			vg = v_g_tab[*(voff+i)];
			vr = v_r_tab[*(voff+i)];

			b = yy + ub;
			g = yy - ug - vg;
			r = yy + vr;

			rgb = r_2_pix[r] + g_2_pix[g] + b_2_pix[b];

			yy = *(yoff+(i<<1)+1);
			b = yy + ub;
			g = yy - ug - vg;
			r = yy + vr;

			pdst[(j*dst_ystride+i)] = (rgb)+((r_2_pix[r] + g_2_pix[g] + b_2_pix[b])<<16);

			yy = *(yoff+(i<<1)+src_ystride);
			b = yy + ub;
			g = yy - ug - vg;
			r = yy + vr;

			rgb = r_2_pix[r] + g_2_pix[g] + b_2_pix[b];

			yy = *(yoff+(i<<1)+src_ystride+1);
			b = yy + ub;
			g = yy - ug - vg;
			r = yy + vr;

			pdst [((2*j+1)*dst_ystride+i*2)>>1] = (rgb)+((r_2_pix[r] + g_2_pix[g] + b_2_pix[b])<<16);
		}
	}
}


int CH264Decoder::GetYUVBuffer(
	unsigned char * dst,
	int size,
	int ow,
	int oh
){
    if(dst == NULL){
        return 0;
    }

	unsigned char * Y = m_pFrame->data[0];
	unsigned char * U = m_pFrame->data[1];
	unsigned char * V = m_pFrame->data[2];

	int iw = m_pCodecCtx->width;
	int ih = m_pCodecCtx->height;

	int w_offset = (iw - ow) / 2;
	int h_offset = (ih - oh) / 2;

	int o_u_offset = ow * oh;
	int o_v_offset = ow * oh + ow * oh / 4;

	int i_u_offset = iw * ih;
	int i_v_offset = iw * ih + iw * ih / 4;

	if(ow > m_pCodecCtx->width 
	|| oh > m_pCodecCtx->height
	){
		return 0;
	}

	// copy directly
	if(ow == m_pCodecCtx->width 
	&& oh == m_pCodecCtx->height
	){
		memcpy(&dst[0],m_pFrame->data[0],ow * oh);
		memcpy(&dst[o_u_offset],m_pFrame->data[1],ow * oh/4);
		memcpy(&dst[o_v_offset],m_pFrame->data[2],ow * oh/4);
		return 1;
	}

	// copy crop area
	if(ow % 2 != 0 
	|| oh % 2 != 0
	){
		return 0;
	}

	for(int row = 0;row < oh;row ++){
		memcpy(&dst[ow * row],&Y[(h_offset + row) * iw + w_offset],ow);
	}

	int k = 0;
    for (int row = 0; row < oh; row += 2) {
        for (int col = 0; col < ow; col += 2) {
            int idx = (h_offset + row) * iw / 4 + (w_offset + col) / 2;
            dst[o_u_offset + k] = U[idx];
            dst[o_v_offset + k] = V[idx];
            k++;
        }
    }
	
    return 1;
}

int CH264Decoder::DecoderFrame(uint8_t *pbuf, int len, int &width, int &height,int isKeyFrame)
{
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = pbuf;
    avpkt.size = len;
	avpkt.flags = isKeyFrame ? AV_PKT_FLAG_KEY : AV_PKT_FLAG_CORRUPT;

	int ret = 0;
	int consumed_bytes = 0;

#ifdef FUCKING_FFMPEG_GROUP
	ret = avcodec_decode_video2(m_pCodecCtx, m_pFrame, &consumed_bytes, &avpkt);
	if(ret <= 0) return ret;
#else
	ret = avcodec_send_packet(m_pCodecCtx,&avpkt);
	if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
		av_packet_unref(&avpkt);
		return ret;
	}

	ret = avcodec_receive_frame(m_pCodecCtx,m_pFrame);
	if(ret < 0 && ret != AVERROR_EOF){
		av_packet_unref(&avpkt);
		return ret;
	}

	consumed_bytes = m_pFrame->pkt_size;
#endif

	width = m_pFrame->width;
    height = m_pFrame->height;

	if(consumed_bytes > 0){
        return consumed_bytes;
    }
        
    return 0;
}

void CH264Decoder::YUV4202RGB565(uint8_t *yuv420, uint8_t *rgb565, int width, int height)
{
    unsigned char *y = yuv420;
    unsigned char *u = yuv420 + width * height;
    unsigned char *v = yuv420 + width *height * 5 / 4;

    DisplayYUV_16((unsigned int*)rgb565, y, u, v, 
							width, height, width, width/2
							,width);
}


//Ãÿ ‚◊˜”√µƒ∑Ω∑®
void CH264Decoder::YUV4202RGB565(uint8_t *out)
{
    DisplayYUV_16((unsigned int*)out, m_pFrame->data[0], m_pFrame->data[1], m_pFrame->data[2], 
							m_pCodecCtx->width, m_pCodecCtx->height, m_pFrame->linesize[0], m_pFrame->linesize[1]
							, m_pCodecCtx->width);
}

void CH264Decoder::YUV420CUTSIZE(
	char * src,
	char * dst,
	int iw,
	int ih,
	int ow,
	int oh
){
	if(ow > iw || oh > ih) return;
	if(ow % 2 != 0 
	|| oh % 2 != 0
	){
		return;
	}

//	int i_copy_lens = iw * ih;
//	int o_copy_lens = ow * oh;
	int w_offset = (iw - ow) / 2;
	int h_offset = (ih - oh) / 2;

	int o_u_offset = ow * oh;
	int o_v_offset = ow * oh + ow * oh / 4;

	int i_u_offset = iw * ih;
	int i_v_offset = iw * ih + iw * ih / 4;

	for(int line = 0;line < oh;line ++){
		memcpy(&dst[ow * line],&src[(h_offset + line) * iw + w_offset],ow);
	}
	
	int k = 0;
    for (int row = 0; row < oh; row += 2) {
        for (int col = 0; col < ow; col += 2) {
            int old_index = (h_offset + row) * iw / 4 + (w_offset + col) / 2;
            dst[o_u_offset + k] = src[i_u_offset + old_index];
            dst[o_v_offset + k] = src[i_v_offset + old_index];
            k++;
        }
    }
	

	/*
	for(int line = 0;line < oh/4; line ++){
		memcpy(
			&dst[o_u_offset + ow * line],
			&src[i_u_offset + (h_offset + line) * iw + w_offset],
			ow);
		
		memcpy(
			&dst[o_v_offset + ow * line],
			&src[i_v_offset + (h_offset + line) * iw + w_offset],
			ow);
	};
	*/
}

CH264Decoder::~CH264Decoder()
{
    if(m_pFrame){
        av_frame_free(&m_pFrame);
        m_pFrame = NULL;
    }

    if(m_pCodecCtx){
        avcodec_close(m_pCodecCtx);
        avcodec_free_context(&m_pCodecCtx);
        m_pCodecCtx = NULL;   
    }
    
    DeleteYUVTab();   
}



