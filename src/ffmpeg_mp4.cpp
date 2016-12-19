
#include "ffmpeg_mp4.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
    
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
    
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavcodec/avcodec.h>
    
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
    
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
    
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/avstring.h>
#include <libavutil/mem.h>
    
#ifdef __cplusplus
}
#endif

    
/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
       Log2(" No memory");
	   return NULL;
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            Log2( "Error allocating an audio buffer\n");
           av_frame_free(&frame);
	   frame = NULL;
        }
    }

    return frame;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{

 //	Log2("frame pts=%lld, dts=%lld", pkt->pts, pkt->dts);
}



int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
   
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
int  add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        Log2( "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return 0;
    }

    Log2("codec id = %d",codec_id);

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st) {
        Log2("Could not allocate stream\n");
        return 0;
    }
    ost->st->id = oc->nb_streams-1;
    c = ost->st->codec;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:

	c->codec_id = codec_id;

        c->sample_fmt  = AV_SAMPLE_FMT_FLT;//AV_SAMPLE_FMT_S16;
        c->bit_rate    = 32000; //64000
        c->sample_rate = 8000;//44100;
       
	c->channel_layout = AV_CH_LAYOUT_MONO;
        c->channels        =  1; //av_get_channel_layout_nb_channels(c->channel_layout);
            
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        
        c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

      break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

    //    c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = 1280;
        c->height   = 720;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
        c->time_base       = ost->st->time_base;

     //   c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = AV_PIX_FMT_YUV420P;
        
    break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    return 0;
}



/* Add an output stream. */
int  add_video_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec, enum AVCodecID codec_id, int video_width, int video_height, int fps)
{
	AVCodecContext *c;

	memset(ost,0, sizeof(*ost));

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
	    Log2( "Could not find encoder for '%s'\n",
	            avcodec_get_name(codec_id));
	    return -1;
	}

	Log2("codec id = %d",codec_id);

	ost->st = avformat_new_stream(oc, *codec);
	if (!ost->st) {
	    	Log2("Could not allocate stream\n");
	    	return -1;
	}
	ost->st->id = oc->nb_streams-1;
	c = ost->st->codec;

	if((*codec)->type != AVMEDIA_TYPE_VIDEO)
	{
		Log2("add_video_stream error, invalid codec type: not video");
		return -1;
	}

	c->codec_id = codec_id;

	/* Resolution must be a multiple of two. */
	c->width    = video_width;
	c->height   = video_height;
	/* timebase: This is the fundamental unit of time (in seconds) in terms
	 * of which frame timestamps are represented. For fixed-fps content,
	 * timebase should be 1/framerate and timestamp increments should be
	 * identical to 1. */
	ost->st->time_base = (AVRational){ 1, fps };
	c->time_base       = ost->st->time_base;

	//   c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
	c->pix_fmt       = AV_PIX_FMT_YUV420P;
	  
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
	    c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return 0;
}


/* Add an output stream. */
int  add_audio_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,  enum AVCodecID codec_id, int sample_rate, int channels)
{
	AVCodecContext *c;

	memset(ost,0, sizeof(*ost));

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
	    Log2( "Could not find encoder for '%s'\n",
	            avcodec_get_name(codec_id));
	    return -1;
	}

	Log2("codec id = %d",codec_id);

	ost->st = avformat_new_stream(oc, *codec);
	if (!ost->st) {
		Log2("Could not allocate stream\n");
	    	return -1;
	}
	ost->st->id = oc->nb_streams-1;
	c = ost->st->codec;

	if ((*codec)->type != AVMEDIA_TYPE_AUDIO) 
	{
		Log2("add_audio_stream error, invalid codec type: not audio");
		return -1;
	}
    

	c->codec_id = codec_id;

	c->sample_fmt  = AV_SAMPLE_FMT_FLT;//AV_SAMPLE_FMT_S16;
	c->bit_rate    = 32000; //64000
	c->sample_rate = sample_rate;//44100;

	c->channels        = channels;          // //av_get_channel_layout_nb_channels(c->channel_layout);

	c->channel_layout =( channels==1?AV_CH_LAYOUT_MONO:AV_CH_LAYOUT_STEREO);
		    
	ost->st->time_base = (AVRational){ 1, c->sample_rate };

	c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
     
	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        	c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return 0;
}



int open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->st->codec;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);
	
    /* open the codec */
    ret = avcodec_open2(c, codec, NULL/*&opt*/);
    av_dict_free(&opt);
    if (ret < 0) {
        Log2( "Could not open video codec: %s\n", av_err2str(ret));
    }
   else
   {
   	Log2("Open video codec OK\n");

   }
			
   return ret;
  }



int open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int ret;
    AVDictionary *opt = NULL;
 //   int size;

    c = ost->st->codec;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        Log2( "Could not open audio codec: %s\n", av_err2str(ret));
	goto done;
  
    }
   else
   {
   	Log2("Open audio codec OK, sample_fmt= %d, sample_rate=%d, frame_size= %d\n",
		c->sample_fmt,c->sample_rate, c->frame_size);
   }

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, c->frame_size);

    if(NULL == ost->frame)
    {
    	Log2("alloc_audio_frame error, no memory");
	ret = -ENOMEM;
	goto done;
    }
	
    ost->tmp_frame	   = alloc_audio_frame(c->sample_fmt, c->channel_layout,
										  c->sample_rate, c->frame_size);

   if(NULL == ost->tmp_frame)
    {
    	av_frame_free(&ost->frame);
	ret = -ENOMEM;
	
    	Log2("alloc_audio_frame error, no memory");
	
	goto done;
    }
   
 //   size = av_samples_get_buffer_size(NULL, c->channels,c->frame_size,c->sample_fmt, 1);


 /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        Log2( "Could not allocate resampler context\n");
        return -1;
    }


    /* set options */
    av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);



    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        Log2("Failed to initialize the resampling context\n");
        return ret;
    }

done:	
   return ret;
}



/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
int write_audio_frame(AVFormatContext *oc, 
	OutputStream *ost, unsigned char* paudio_data,int audio_len, long long pts)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;
//	int i;

    av_init_packet(&pkt);
    c = ost->st->codec;

    frame = ost->tmp_frame;

	
   //avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt,(const uint8_t*)paudio_data, audio_len, 1);
   memcpy(frame->data[0],paudio_data,audio_len);
   
    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    Log2(" original frame pts = %d\n", frame->pts);


    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
            /* compute destination number of samples */
            dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                            c->sample_rate, c->sample_rate, AV_ROUND_UP);
            av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
        {
        	Log2(" av_frame_make_writable err = %d",ret);
        }

            /* convert to destination format */
            ret = swr_convert(ost->swr_ctx,
                              ost->frame->data, dst_nb_samples,
                              (const uint8_t **)frame->data, frame->nb_samples);
            if (ret < 0) {
		Log2("Error while converting\n",ret);
                return ret;
            }
            frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }


    Log2("audio frame pts = %d", frame->pts);

    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        Log2( "Error encoding audio frame: %s\n", av_err2str(ret));
        return ret;
    }

    if (got_packet) {

	Log2("audio encoder ret len = %d, pts=%ld\n", pkt.size,pkt.pts);

	pkt.pts = pkt.dts = frame->pts;
	pkt.stream_index = ost->st->index;
	
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
            Log2( "Error while writing audio frame: %s\n",
                    av_err2str(ret));
           return ret;
        }
    }

    av_free_packet(&pkt);
	
    return ret;
}

void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_close(ost->st->codec);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}



