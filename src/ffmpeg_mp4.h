
#ifndef __FFMPEG_MP4_H_
#define __FFMPEG_MP4_H_

#include <sys/resource.h>

#ifdef PLATFORM_ANDROID
#include <sys/system_properties.h>
#include <sys/prctl.h>
#endif

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

#include "H264Decoder.h"

#ifdef _cplusplus_
extern "C" {
#endif
    
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
    
#include <inttypes.h>
//#include <libavutil/timestamp.h>

#include "utility.h"


// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;
    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

#define STREAM_FRAME_RATE 25

int  add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id);

int  add_video_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec, enum AVCodecID codec_id, int video_width, int video_height, int fps);
int  add_audio_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,  enum AVCodecID codec_id, int sample_rate, int channels);


int open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
int open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);

int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);
void close_stream(AVFormatContext *oc, OutputStream *ost);


int write_audio_frame(AVFormatContext *oc, 
	OutputStream *ost, unsigned char* paudio_data,int audio_len, long long pts);


#ifdef _cplusplus_
}
#endif


#endif
