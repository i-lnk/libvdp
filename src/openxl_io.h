/*
opensl_io.c:
Android OpenSL input/output module header
Copyright (c) 2012, Victor Lazzarini
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _OPENXL_IO_H_
#define _OPENXL_IO_H_

#define	CBC_CACHE_NUM	   3
#define AEC_CACHE_LEN	   160

#ifdef PLATFORM_ANDROID

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#else

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

typedef struct SLSimpleBufferQueue{
    void  * OXLPtr;
    void (*Enqueue)(SLSimpleBufferQueue ** bq,char * buffer,int lens);
}SLSimpleBufferQueue,**SLAndroidSimpleBufferQueueItf;

#endif

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct openxl_stream {
    
#ifdef PLATFORM_ANDROID
  
    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;

    // output mix interfaces
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLEffectSendItf bqPlayerEffectSend;

    // recorder interfaces
    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;

    SLVolumeItf bqPlayerVolume;

#else
    
    ALuint          sourceID;
    ALCcontext *    alctx;
    ALCdevice *     aldev;
    pthread_t       altid;
    
    volatile char   exit;
    
#endif
    
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    void (*bqRecordCallback)(SLAndroidSimpleBufferQueueItf bq, void *context);
    void (*bqPlayerCallback)(SLAndroidSimpleBufferQueueItf bq, void *context);

    long long   time;
    int 		ichannels;
    int 		ochannels;
    int         sr;

    void *      context;

    short *     recordBuffer;
    short *     outputBuffer;

    int         oBufferIndex;
    int         iBufferIndex;

} OPENXL_STREAM;

  /*
  Open the audio device with a given sampling rate (sr), input and output channels and IO buffer size
  in frames. Returns a handle to the OpenSL stream
  */
OPENXL_STREAM * InitOpenXLStream(int 	sr,
	int 	ichannels, 
	int 	ochannels, 
	void * 	context,
	void  (*bqRecordCallback)(SLAndroidSimpleBufferQueueItf,void *),
	void  (*bqPlayerCallback)(SLAndroidSimpleBufferQueueItf,void *)
);
    
  /* 
  Close the audio device 
  */
void FreeOpenXLStream(OPENXL_STREAM *p);
  
#ifdef __cplusplus
};
#endif

#endif // #ifndef OPENSL_IO
