#include "utility.h"

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

AVCodecContext   *g_pCodecCtx;
AVCodec       *g_pCodec;
AVFrame       *g_pFrame;

void global_init_decode()
{
    //Log("Call init_decode()");
    
    av_register_all();      

    g_pCodec = avcodec_find_decoder(CODEC_ID_H264);
    if(g_pCodec == NULL)
    {
        Log("pCodec == NULL\n");
        return ;
    }
    //Log("avcodec_find_decoder ok");

    g_pCodecCtx = avcodec_alloc_context3(g_pCodec);
    if(g_pCodecCtx == NULL)
    {
        Log("if(pCodecCtx == NULL)\n");
        return ;
    }  
    //Log("avcodec_alloc_context3 ok");
    
  
    //´ò¿ªcodec¡£Èç¹û´ò¿ª³É¹¦µÄ»°£¬·ÖÅäAVFrame£     
    if(avcodec_open2(g_pCodecCtx, g_pCodec, NULL) >= 0)     
    {  
        //Log("avcodec_open2 ok");
        g_pFrame = av_frame_alloc();   /* Allocate video frame   */
        //Log("avcodec_alloc_frame ok");
        
    }   
    
}

void global_free_decoder()
{
    if(g_pFrame)
    {
        av_frame_free(&g_pFrame);
        g_pFrame = NULL;
    }

    if(g_pCodecCtx)	
    {       
        avcodec_close(g_pCodecCtx);       
        g_pCodecCtx = NULL;   
    }    
}

static int check_size(int width , int height)
{
    if(width <= 0 || width > 3000 || height <= 0 || height > 2000)
    {
        return 0;
    }

    return 1;
}

int global_decode_one_frame_with_buf(char*pbuf, int len, char *yuv, int *width, int *height)
{
    if(yuv == NULL || pbuf == NULL)
    {
        return 0;
    }

    *width = 0;
    *height = 0;
    
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (uint8_t*)pbuf;
    avpkt.size = len;

    
    int consumed_bytes = 0;
	avcodec_decode_video2(g_pCodecCtx, g_pFrame, &consumed_bytes, &avpkt);    

    if(consumed_bytes <= 0)
    {        
        return consumed_bytes ;
    }

    //Log("g_pCodecCtx->width: %d, height: %d", g_pCodecCtx->width, g_pCodecCtx->height);
    int ww, hh;
    ww = g_pCodecCtx->width;
    hh = g_pCodecCtx->height;

    if(0 == check_size(ww, hh))
    {
        return 0;
    }
    
    *width = ww ;
    *height = hh ;    

    char *y = yuv;
    char *u = yuv + ww * hh;
    char *v = yuv + ww * hh * 5 / 4;

    char *p = NULL;

    int i,j,k;
    for(i=0;i<hh;i++)
    {
        p = y + i *  ww;
        memcpy(p, g_pFrame->data[0]+g_pFrame->linesize[0]*i, ww);
    }

    for(j=0;j<hh/2;j++)
    {
        p = u + j *  ww/2;
        memcpy(p, g_pFrame->data[1]+g_pFrame->linesize[1]*j, ww/2);
    }

    for(k=0;k<hh/2;k++)
    {
        p = v + k *  ww/2 ;
        memcpy(p, g_pFrame->data[2]+g_pFrame->linesize[2]*k,  ww/2);
    }    
    
    return consumed_bytes;
}


int global_decode_one_frame(char*pbuf, int len, char **y, char** u, char **v, int *width, int *height)
{
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (uint8_t*)pbuf;
    avpkt.size = len;

    *y = NULL;
    *u = NULL;
    *v = NULL;
    *width = 0;
    *height = 0;

    if(y == NULL || u == NULL || v == NULL)
    {
        return 0;
    }
    
    int consumed_bytes = 0;
	avcodec_decode_video2(g_pCodecCtx, g_pFrame, &consumed_bytes, &avpkt);    

    if(consumed_bytes <= 0)
    {        
        return consumed_bytes ;
    }

    //Log("g_pCodecCtx->width: %d, height: %d", g_pCodecCtx->width, g_pCodecCtx->height);
    int ww, hh;
    ww = g_pCodecCtx->width;
    hh = g_pCodecCtx->height;

    if(0 == check_size(ww, hh))
    {
        return 0;
    }
    
    *width = ww ;
    *height = hh ;    

    //return consumed_bytes;

    char *_y = new char[ww * hh];
    char *_u = new char[ww * hh / 4] ;
    char *_v = new char[ww * hh / 4] ;

    *y = _y;
    *u = _u;
    *v = _v;

    char *p;

    int i,j,k;
    for(i=0;i<hh;i++)
    {
        p = _y + i *  ww;
        memcpy(p, g_pFrame->data[0]+g_pFrame->linesize[0]*i, ww);
    }

    for(j=0;j<hh/2;j++)
    {
        p = _u + j *  ww/2;
        memcpy(p, g_pFrame->data[1]+g_pFrame->linesize[1]*j, ww/2);
    }

    for(k=0;k<hh/2;k++)
    {
        p = _v + k *  ww/2 ;
        memcpy(p, g_pFrame->data[2]+g_pFrame->linesize[2]*k,  ww/2);
    }    
    
    return consumed_bytes;
}

