#ifdef PLATFORM_ANDROID
#include <jni.h>
#endif

#include "H264Decoder.h"
#include "utility.h"

CH264Decoder::CH264Decoder()
{
    //F_LOG ;    

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

    m_pCodec = avcodec_find_decoder(CODEC_ID_H264);
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
  
    //´ò¿ªcodec¡£Èç¹û´ò¿ª³É¹¦µÄ»°£¬·ÖÅäAVFrame£     
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

	for(j=0; j<height2; j++) // Ò»´Î2x2¹²ËÄ¸öÏñËØ
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


int CH264Decoder::GetYUVBuffer(uint8_t * pYUVBuffer,int bufLen)
{
    if(pYUVBuffer == NULL)
    {
        return 0;
    }

	int CopySize = m_pCodecCtx->width * m_pCodecCtx->height;

	unsigned char * P = pYUVBuffer;
	
	memcpy(&P[0],m_pFrame->data[0],CopySize);
	memcpy(&P[CopySize],m_pFrame->data[1],CopySize/4);
	memcpy(&P[CopySize + CopySize/4],m_pFrame->data[2],CopySize/4);
	
	/*
    int iWidth = m_pCodecCtx->width ;
    int iHeight = m_pCodecCtx->height ;
    
     int i, j, k;    
     unsigned char *p = pYUVBuffer;
     for(i=0;i<iHeight;i++)        
     {
        memcpy(p, m_pFrame->data[0]+m_pFrame->linesize[0]*i, iWidth);
        p += iWidth ;
     }

     for(j=0;j<iHeight/2;j++)   
     {
        memcpy(p, m_pFrame->data[1]+m_pFrame->linesize[1]*j, iWidth/2);
        p += iWidth/2 ; 
     }

     for(k=0;k<iHeight/2;k++)      
     {
        memcpy(p, m_pFrame->data[2]+m_pFrame->linesize[2]*k, iWidth/2);
        p += iWidth/2 ; 
     }
     */

	
    return 1;
}

int CH264Decoder::DecoderFrame(uint8_t *pbuf, int len, int &width, int &height,int isKeyFrame)
{
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = pbuf;
    avpkt.size = len;
	avpkt.flags = isKeyFrame ? AV_PKT_FLAG_KEY : AV_PKT_FLAG_CORRUPT;

	int consumed_bytes = 0;
	int ret = avcodec_decode_video2(m_pCodecCtx, m_pFrame, &consumed_bytes, &avpkt);
	if(ret <= 0) return ret;

    //Log("m_pCodecCtx->width: %d, height: %d", m_pCodecCtx->width, m_pCodecCtx->height);
    width = m_pCodecCtx->width ;
    height = m_pCodecCtx->height ;

    if(consumed_bytes > 0)
    {
        return consumed_bytes ;
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


//ÌØÊâ×÷ÓÃµÄ·½·¨
void CH264Decoder::YUV4202RGB565(uint8_t *out)
{
    DisplayYUV_16((unsigned int*)out, m_pFrame->data[0], m_pFrame->data[1], m_pFrame->data[2], 
							m_pCodecCtx->width, m_pCodecCtx->height, m_pFrame->linesize[0], m_pFrame->linesize[1]
							, m_pCodecCtx->width);
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



