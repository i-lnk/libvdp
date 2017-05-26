/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

#include "muxing.h"
#include "utility.h"

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

static int audio_sample_rate = 8000;
static int video_fps = 10;
static int video_w = 640;
static int video_h = 320;

static OutputStream video_st = { 0 }, audio_st = { 0 };
static AVOutputFormat *fmt;
static AVFormatContext *oc;
static AVCodec *audio_codec, *video_codec;
static char errmsg[128] = {0};

static COMMO_LOCK lock = PTHREAD_MUTEX_INITIALIZER;

static int instance = 0;

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static int add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        Log3("Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return -1;
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        Log3("Could not allocate stream\n");
        return -1;
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        Log3("Could not alloc an encoding context\n");
        return -1;
    }
    ost->enc = c;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = AV_SAMPLE_FMT_FLT;
        c->bit_rate    = 32000;
        c->sample_rate = audio_sample_rate;
        c->channel_layout = AV_CH_LAYOUT_MONO;
        c->channels        = 1;
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        //c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width    = video_w;
        c->height   = video_h;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, video_fps };
        c->time_base       = ost->st->time_base;

        c->gop_size      = 50; /* emit one intra frame every twelve frames at most */
        c->pix_fmt       = AV_PIX_FMT_YUV420P;
	    break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return 0;
}

/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        Log3("Error allocating an audio frame\n");
        return NULL;
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            Log3("Error allocating an audio buffer\n");
            return NULL;
        }
    }

    return frame;
}

static int open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->enc;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
		av_strerror(ret,errmsg,sizeof(errmsg));
        Log3("Could not open audio codec: %s\n", errmsg);
        return -1;
    }

    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        Log3("Could not copy the stream parameters\n");
        return -1;
    }

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        Log3("Could not allocate resampler context\n");
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
        Log3("Failed to initialize the resampling context\n");
        return -1;
    }

	return 0;
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
static AVFrame *get_audio_frame(OutputStream *ost,char * frameData,int frameLens)
{
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
	/*
	if (av_compare_ts(ost->next_pts, ost->enc->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;
	*/
	memcpy(q,frameData,frameLens);

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 0 when encoding is finished, -1 otherwise
 */
static int write_audio_frame(
	AVFormatContext *	oc, 
	OutputStream *		ost,
	void * 				frameData,
	int 				frameLens,
	int					frameType
	
){
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;

    av_init_packet(&pkt);
    c = ost->enc;

    frame = get_audio_frame(ost,(char*)frameData,frameLens);

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
            return -1;

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            Log3("Error while converting\n");
            return -1;
        }
        frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }

	ret = avcodec_send_frame(c,frame);
	if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
		av_packet_unref(&pkt);
		return ret;
	}

	ret = avcodec_receive_packet(c,&pkt);
	if(ret < 0 && ret != AVERROR_EOF){
		av_packet_unref(&pkt);
		return ret;
	}else{
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
		
		av_packet_unref(&pkt);
		
        if (ret < 0) {
			av_strerror(ret,errmsg,sizeof(errmsg));
            Log3("Error while writing audio frame: %s ret: %d\n",errmsg,ret);
            return -1;
        }
    }

    return 0;
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        Log3("Could not allocate frame data.\n");
        return NULL;
    }

    return picture;
}

static int open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
		av_strerror(ret,errmsg,sizeof(errmsg));
        Log3("Could not open video codec: %s\n",errmsg);
        return -1;
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        Log3("Could not allocate video frame\n");
        return -1;
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            Log3("Could not allocate temporary picture\n");
            return -1;
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        Log3("Could not copy the stream parameters\n");
        return -1;
    }
    
    return 0;
}

static AVFrame *get_video_frame(OutputStream *ost,char * frameData,int frameLens)
{
    AVCodecContext *c = ost->enc;

	if (frameData == NULL) return NULL;

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        return NULL;

	int CopySize = c->width * c->height;

	memcpy(ost->frame->data[0],&frameData[0],CopySize);
	memcpy(ost->frame->data[1],&frameData[CopySize],CopySize/4);
	memcpy(ost->frame->data[2],&frameData[CopySize + CopySize/4],CopySize/4);

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(
	AVFormatContext *	oc, 
	OutputStream *		ost,
	void * 				frameData,
	int 				frameLens,
	int					frameType
){
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };

    c = ost->enc;

    av_init_packet(&pkt);

    /* encode the image */
#if ENABLE_VIDEO_ENCODE
	frame = get_video_frame(ost,(char*)frameData,frameLens);

	ret = avcodec_send_frame(c,frame);
	if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
		av_packet_unref(&pkt);
		return ret;
	}

	ret = avcodec_receive_packet(c,&pkt);
	if(ret < 0 && ret != AVERROR_EOF){
		av_packet_unref(&pkt);
		return ret;
	}
#else
	pkt.pts = pkt.dts = ost->next_pts++;
	pkt.stream_index = ost->st->index;
	pkt.data = (uint8_t*)frameData;
	pkt.size = frameLens;
	pkt.flags |= frameType ? AV_PKT_FLAG_KEY : 0;
	pkt.duration = 0;
#endif

	ret = write_frame(oc, &c->time_base, ost->st, &pkt);

	av_packet_unref(&pkt);

    if (ret < 0) {
		av_strerror(ret,errmsg,sizeof(errmsg));
        Log3("Error while writing video frame: %s\n", errmsg);
        return -1;
    }

    return 0;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */
#if 0
int main(int argc, char **argv)
{
    const char *filename;
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;

    /* Initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    if (argc < 2) {
        Log3("usage: %s output_file\n"
               "API example program to output a media file with libavformat.\n"
               "This program generates a synthetic audio and video stream, encodes and\n"
               "muxes them into a file named output_file.\n"
               "The output format is automatically guessed according to the file extension.\n"
               "Raw images can also be output by using '%%d' in the filename.\n"
               "\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    for (i = 2; i+1 < argc; i+=2) {
        if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
            av_dict_set(&opt, argv[i]+1, argv[i+1], 0);
    }

    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        Log3("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);

    if (have_audio)
        open_audio(oc, audio_codec, &audio_st, opt);

    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            Log3("Could not open '%s': %s\n", filename,
                    av_strerror(ret,errmsg,sizeof(errmsg)));
            return 1;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        Log3("Error occurred when opening output file: %s\n",
                av_strerror(ret,errmsg,sizeof(errmsg)));
        return 1;
    }

    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                                            audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &video_st);
        } else {
            encode_audio = !write_audio_frame(oc, &audio_st);
        }
    }

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);

    /* Close each codec. */
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}
#endif

int WriteFrameType(){

	GET_LOCK(&lock);
	
	if(!instance){
		PUT_LOCK(&lock);
		return -1;
	}

	if(av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                     audio_st.next_pts, audio_st.enc->time_base) <= 0){
        PUT_LOCK(&lock);
		return 1;
    }

	PUT_LOCK(&lock);

	return 0;
}

int StartRecording(
	char * 		filename,
	int			fps,
	int			width,
	int			height,
	int			audio_sample_rate,
	int  *  	audio_length
){

	GET_LOCK(&lock);

	AVDictionary *opt = NULL;
	int ret = -1;

	video_fps = fps;
	video_w = width;
	video_h = height;

	/* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        Log3("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc) {
		Log3("Could not deduce output format from file extension.\n");
        goto jumperr;
    }
	
	fmt = oc->oformat;
	fmt->video_codec = AV_CODEC_ID_H264;
	fmt->audio_codec = AV_CODEC_ID_AAC;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
    }

	 /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */

    open_video(oc, video_codec, &video_st, opt);
    open_audio(oc, audio_codec, &audio_st, opt);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
	   ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
	   if (ret < 0) {
	   		av_strerror(ret,errmsg,sizeof(errmsg));
		   	Log3("Could not open '%s': %s\n", filename, errmsg);
		   	goto jumperr;
	   }
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {
		av_strerror(ret,errmsg,sizeof(errmsg));
	   	Log3("Error occurred when opening output file: %s\n",errmsg);
	   	goto jumperr;
	}

	*audio_length = audio_st.enc->frame_size * 2;

	instance = 1;

	PUT_LOCK(&lock);

	return  0;

jumperr:
	
	if(oc){
		close_stream(oc,&video_st);
		close_stream(oc,&audio_st);
	}

	memset(&video_st,0,sizeof(video_st));
	memset(&audio_st,0,sizeof(audio_st));

	instance = 0;
	
	PUT_LOCK(&lock);
	
	return -1;
}

int CloseRecording(){
	GET_LOCK(&lock);

	if(!instance) goto jumpout;
	if(!oc) goto jumpout;

	av_write_trailer(oc);

	close_stream(oc,&video_st);
	close_stream(oc,&audio_st);

	if(!(fmt->flags & AVFMT_NOFILE)){
        /* Close the output file. */
        avio_closep(&oc->pb);
	}
	
    /* free the stream */
    avformat_free_context(oc);

	instance = 0;

jumpout:
	
	PUT_LOCK(&lock);

	return 0;
}

int WriteRecordingFrames(
	void * 		frameData,
	int 		frameLens,
	int			frameCode,
	int			frameType,
	long long 	ts
){
	int err = -1;

	GET_LOCK(&lock);
	
	if(!instance) goto jumperr;

	switch(frameCode){
		case 0:
			err = write_audio_frame(oc,&audio_st,frameData,frameLens,frameType);
			break;
		case 1:
			err = write_video_frame(oc,&video_st,frameData,frameLens,frameType);
			break;
	}

jumperr:
	PUT_LOCK(&lock);
	
	return err;
}

