/*
  opensl_io.c:
  Android OpenSL input/output module
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utility.h"
#include "openxl_io.h"

#ifdef PLATFROM_ANDROID

#include <jni.h>

// creates the OpenSL ES audio engine
static SLresult openSLCreateEngine(OPENSL_STREAM *p)
{
	SLEngineOption EngineOption[] = {
		(SLuint32) SL_ENGINEOPTION_THREADSAFE,
		(SLuint32) SL_BOOLEAN_TRUE
	};

	SLresult result;
	// create engine
	result = slCreateEngine(&(p->engineObject), 0, EngineOption, 0, NULL, NULL);
	if(result != SL_RESULT_SUCCESS) goto engine_end;

	// realize the engine 
	result = (*p->engineObject)->Realize(p->engineObject, SL_BOOLEAN_FALSE);
	if(result != SL_RESULT_SUCCESS) goto engine_end;

	// get the engine interface, which is needed in order to create other objects
	result = (*p->engineObject)->GetInterface(p->engineObject, SL_IID_ENGINE, &(p->engineEngine));
	if(result != SL_RESULT_SUCCESS) goto engine_end;

engine_end:
	return result;
}

// close the OpenSL IO and destroy the audio engine
static void openSLDestroyEngine(OPENSL_STREAM *p){

  // destroy buffer queue audio player object, and invalidate all associated interfaces
  if (p->bqPlayerObject != NULL) {
    SLuint32 state = SL_PLAYSTATE_PLAYING;
    (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_STOPPED);
	Log2("wait for player stop.");
    while(state != SL_PLAYSTATE_STOPPED){
      (*p->bqPlayerPlay)->GetPlayState(p->bqPlayerPlay, &state);
	  usleep(100);
    }
    (*p->bqPlayerObject)->Destroy(p->bqPlayerObject);
    p->bqPlayerObject = NULL;
    p->bqPlayerPlay = NULL;
	p->bqPlayerVolume = NULL;
    p->bqPlayerBufferQueue = NULL;
    p->bqPlayerEffectSend = NULL;
  }

  // destroy audio recorder object, and invalidate all associated interfaces
  if (p->recorderObject != NULL) {
   SLuint32 state = SL_PLAYSTATE_PLAYING;
    (*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_STOPPED);
	Log2("wait for record stop.");
    while(state != SL_RECORDSTATE_STOPPED){
      (*p->recorderRecord)->GetRecordState(p->recorderRecord, &state);
	  usleep(100);
    }
    (*p->recorderObject)->Destroy(p->recorderObject);
    p->recorderObject = NULL;
    p->recorderRecord = NULL;
    p->recorderBufferQueue = NULL;
  }

  Log2("destory mix object.");

  // destroy output mix object, and invalidate all associated interfaces
  if (p->outputMixObject != NULL) {
    (*p->outputMixObject)->Destroy(p->outputMixObject);
    p->outputMixObject = NULL;
  }

  Log2("destory engine object.");

  // destroy engine object, and invalidate all associated interfaces
  if (p->engineObject != NULL) {
    (*p->engineObject)->Destroy(p->engineObject);
    p->engineObject = NULL;
    p->engineEngine = NULL;
  }
  if (p->outputBuffer != NULL) {
    free(p->outputBuffer);
    p->outputBuffer = NULL;
  }

  if (p->recordBuffer != NULL) {
    free(p->recordBuffer);
    p->recordBuffer = NULL;
  }

  Log2("destory all opensl resource done.");

}

// opens the OpenSL ES device for output
static SLresult openSLPlayerOpen(OPENSL_STREAM *p)
{
  SLresult result;
  SLuint32 sr = p->sr;
  SLuint32  channels = p->ochannels;
  char *pBuffer = NULL;
  if(channels){
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    switch(sr){

    case 8000:
      sr = SL_SAMPLINGRATE_8;
      break;
    case 11025:
      sr = SL_SAMPLINGRATE_11_025;
      break;
    case 16000:
      sr = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      sr = SL_SAMPLINGRATE_22_05;
      break;
    case 24000:
      sr = SL_SAMPLINGRATE_24;
      break;
    case 32000:
      sr = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      sr = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      sr = SL_SAMPLINGRATE_48;
      break;
    case 64000:
      sr = SL_SAMPLINGRATE_64;
      break;
    case 88200:
      sr = SL_SAMPLINGRATE_88_2;
      break;
    case 96000:
      sr = SL_SAMPLINGRATE_96;
      break;
    case 192000:
      sr = SL_SAMPLINGRATE_192;
      break;
    default:
      return -1;
    }
   
    const SLInterfaceID ids[] = {SL_IID_VOLUME};
    const SLboolean req[] = {SL_BOOLEAN_FALSE};
    result = (*p->engineEngine)->CreateOutputMix(p->engineEngine, &(p->outputMixObject), 1, ids, req);
    if(result != SL_RESULT_SUCCESS){
		Log3("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.");
		return result;
    }

    // realize the output mix
    result = (*p->outputMixObject)->Realize(p->outputMixObject, SL_BOOLEAN_FALSE);

#ifdef _SET_OPENSL_VOLUME_
	// set output volume
	result = (*p->outputMixObject)->GetInterface(p->outputMixObject, SL_IID_VOLUME, &(p->bqPlayerVolume));
	if(result != SL_RESULT_SUCCESS){
		Log3("IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII.");
		return result;
	}
#endif
   
    int speakers;

    if(channels > 1) 
      	speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else 
		speakers = SL_SPEAKER_FRONT_CENTER;

    SLDataFormat_PCM format_pcm = {
					SL_DATAFORMAT_PCM,
					channels, 
					sr,
				   	SL_PCMSAMPLEFORMAT_FIXED_16, 
				   	SL_PCMSAMPLEFORMAT_FIXED_16,
				   	speakers, 
				   	SL_BYTEORDER_LITTLEENDIAN
				   	};

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, p->outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req1[] = {SL_BOOLEAN_TRUE};
    result = (*p->engineEngine)->CreateAudioPlayer(p->engineEngine, &(p->bqPlayerObject), &audioSrc, &audioSnk,
						   1, ids1, req1);
    if(result != SL_RESULT_SUCCESS) return result;
    // realize the player
    result = (*p->bqPlayerObject)->Realize(p->bqPlayerObject, SL_BOOLEAN_FALSE);
    if(result != SL_RESULT_SUCCESS) return result;

#ifdef _SET_OPENSL_VOLUME_
	SLmillibel vol;

	(*p->bqPlayerVolume)->GetMaxVolumeLevel(p->bqPlayerVolume,&vol);
	(*p->bqPlayerVolume)->SetVolumeLevel(p->bqPlayerVolume,vol);

	Log3("MMMMMMMMMMMMMAX VOLUME OPENSL:[%d].",vol);
#endif

    // get the play interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_PLAY, &(p->bqPlayerPlay));
    if(result != SL_RESULT_SUCCESS) return result;
	
    // get the buffer queue interface
    result = (*p->bqPlayerObject)->GetInterface(p->bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
						&(p->bqPlayerBufferQueue));
    if(result != SL_RESULT_SUCCESS) return result;

    // register callback on the buffer queue
    result = (*p->bqPlayerBufferQueue)->RegisterCallback(p->bqPlayerBufferQueue, p->bqPlayerCallback, p);
    if(result != SL_RESULT_SUCCESS) return result;

    // set the player's state to playing
    result = (*p->bqPlayerPlay)->SetPlayState(p->bqPlayerPlay, SL_PLAYSTATE_PLAYING);

	// send 20ms data when start
    if((p->outputBuffer = (short *) calloc(CBC_CACHE_NUM*AEC_CACHE_LEN, sizeof(char))) == NULL) {
      return -1;
    }
    pBuffer =  (char*)p->outputBuffer;
    p->outBufIndex = 0;
    for(int i = 0;i< CBC_CACHE_NUM;i++)
    {
    (*p->bqPlayerBufferQueue)->Enqueue(p->bqPlayerBufferQueue, 
				       pBuffer,AEC_CACHE_LEN*sizeof(char));
     pBuffer+=AEC_CACHE_LEN;
    }

    return result;
  }
  return SL_RESULT_SUCCESS;
}

// Open the OpenSL ES device for input
static SLresult openSLRecordOpen(OPENSL_STREAM *p){

  SLresult result;
  SLuint32 sr = p->sr;
  SLuint32 channels = p->ichannels;
  char *pBuffer = NULL;
  if(channels){

    switch(sr){

    case 8000:
      sr = SL_SAMPLINGRATE_8;
      break;
    case 11025:
      sr = SL_SAMPLINGRATE_11_025;
      break;
    case 16000:
      sr = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      sr = SL_SAMPLINGRATE_22_05;
      break;
    case 24000:
      sr = SL_SAMPLINGRATE_24;
      break;
    case 32000:
      sr = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      sr = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      sr = SL_SAMPLINGRATE_48;
      break;
    case 64000:
      sr = SL_SAMPLINGRATE_64;
      break;
    case 88200:
      sr = SL_SAMPLINGRATE_88_2;
      break;
    case 96000:
      sr = SL_SAMPLINGRATE_96;
      break;
    case 192000:
      sr = SL_SAMPLINGRATE_192;
      break;
    default:
      return -1;
    }

	// recv 20ms data when start
	char pcm20ms[320] = {0};
    
    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
				      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    int speakers;
    if(channels > 1) 
      speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else speakers = SL_SPEAKER_FRONT_CENTER;
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, channels, sr,
				   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
				   speakers, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    result = (*p->engineEngine)->CreateAudioRecorder(p->engineEngine, &(p->recorderObject), &audioSrc,
						     &audioSnk, 1, id, req);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

    // realize the audio recorder
    result = (*p->recorderObject)->Realize(p->recorderObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

    // get the record interface
    result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_RECORD, &(p->recorderRecord));
    if (SL_RESULT_SUCCESS != result) goto end_recopen;
 
    // get the buffer queue interface
    result = (*p->recorderObject)->GetInterface(p->recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
						&(p->recorderBufferQueue));
    if (SL_RESULT_SUCCESS != result) goto end_recopen;


    // register callback on the buffer queue
    result = (*p->recorderBufferQueue)->RegisterCallback(p->recorderBufferQueue, p->bqRecordCallback, p);
    if (SL_RESULT_SUCCESS != result) goto end_recopen;

	/* Set the duration of the recording - 20 milliseconds) */
	result = (*p->recorderRecord)->SetDurationLimit(p->recorderRecord, 20);
	if (SL_RESULT_SUCCESS != result) goto end_recopen;
	
    result = (*p->recorderRecord)->SetRecordState(p->recorderRecord, SL_RECORDSTATE_RECORDING);

    if((p->recordBuffer = (short *) calloc(CBC_CACHE_NUM*AEC_CACHE_LEN, sizeof(char))) == NULL) {
      return -1;
    }
    pBuffer =  (char*)p->recordBuffer;
    p->inBufIndex = 0;
    for(int i = 0;i< CBC_CACHE_NUM;i++)
    {
    (*p->recorderBufferQueue)->Enqueue(p->recorderBufferQueue, 
				       pBuffer, AEC_CACHE_LEN*sizeof(char));
     pBuffer+=AEC_CACHE_LEN;
    }
     
  end_recopen: 
    return result;
  }
  else return SL_RESULT_SUCCESS;


}

#else

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <AVFoundation/AVFoundation.h>

static OSStatus recordCallback(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData) {
    OPENXL_STREAM * hOS = (OPENXL_STREAM *)inRefCon;
    
    return noErr;
}


static OSStatus outputCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
    
    OPENXL_STREAM * hOS = (OPENXL_STREAM *)inRefCon;
    AudioComponentInstance * hInst = (AudioComponentInstance *)hOS->hAUInst;
    
    AudioBuffer buffer;
    buffer.mNumberChannels = 1;
    buffer.mDataByteSize = inNumberFrames * 2;
    buffer.mData = malloc(sizeof(buffer.mDataByteSize));
    
    hOS->cbp((char*)buffer.mData,buffer.mDataByteSize,0,hOS);
    
    AudioBufferList bufferArray;
    bufferArray.mNumberBuffers = 1;
    bufferArray.mBuffers[0] = buffer;
    
    int status = AudioUnitRender(
                    *hInst,
                    ioActionFlags,
                    inTimeStamp,
                    inBusNumber,
                    inNumberFrames,
                    &bufferArray);
    
    Log3("audio unit play result:[%d].",status);
    
    return noErr;
}

static int audioUnitOutputOpen(OPENXL_STREAM * p){
    
    int status = 0;
    int flag = 1;
    
    AudioComponentInstance * hInst = (AudioComponentInstance *)p->hAUInst;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output,
                kOBus,
                &flag,
                sizeof(flag));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Enable Output Failed:[%d].",status);
        return -1;
    }
    
    AudioStreamBasicDescription format;
    memset(&format,0,sizeof(format));
    format.mSampleRate = p->sr;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = 1;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;
    format.mBytesPerPacket = 2;
    format.mBytesPerFrame = 2;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                kOBus,
                &format,
                sizeof(format));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Set Format Failed:[%d].",status);
        return -1;
    }
    
    AURenderCallbackStruct cbst;
    cbst.inputProc = outputCallback;
    cbst.inputProcRefCon = p->context;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global,
                kOBus,
                &cbst,
                sizeof(cbst));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Set Callback Failed:[%d].",status);
        return -1;
    }
    
    return 0;
}

static int audioUnitRecordOpen(OPENXL_STREAM * p){
    
    int status = 0;
    int flag = 1;
    
    AudioComponentInstance * hInst = (AudioComponentInstance *)p->hAUInst;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input,
                kIBus,
                &flag,
                sizeof(flag));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Enable Record Failed:[%d].",status);
        return -1;
    }
    
    AudioStreamBasicDescription format;
    memset(&format,0,sizeof(format));
    format.mSampleRate = p->sr;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = 1;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;
    format.mBytesPerPacket = 2;
    format.mBytesPerFrame = 2;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                kIBus,
                &format,
                sizeof(format));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Set Format Failed:[%d].",status);
        return -1;
    }
    
    AURenderCallbackStruct cbst;
    cbst.inputProc = recordCallback;
    cbst.inputProcRefCon = p->context;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioUnitProperty_SetRenderCallback,
                kAudioUnitScope_Global,
                kIBus,
                &cbst,
                sizeof(cbst));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Set Callback Failed:[%d].",status);
        return -1;
    }
    
    flag = 0;
    
    status = AudioUnitSetProperty(
                *hInst,
                kAudioUnitProperty_ShouldAllocateBuffer,
                kAudioUnitScope_Output,
                kIBus,
                &flag,
                sizeof(flag));
    
    if(status != 0){
        Log3("AudioUnitSetProperty Disable Buffer Allocate Failed:[%d].",status);
        return -1;
    }
    
    return 0;
}

static int audioUnitCreateEngine(OPENXL_STREAM * p){
    int err = 0;
    
    p->hAUInst = (void *)malloc(sizeof(AudioComponentInstance));
    memset(p->hAUInst,0,sizeof(AudioComponentInstance));
    
    AudioComponentInstance * hInst = (AudioComponentInstance *)p->hAUInst;
    
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    //  desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    err = AudioComponentInstanceNew(inputComponent,hInst);
    if(err != 0){
        Log3("audio unit instance create failed:[%d].",err);
        goto jumperr;
    }
    
    if(audioUnitRecordOpen(p) != 0 || audioUnitOutputOpen(p) != 0){
        goto jumperr;
    }
    
    err = AudioUnitInitialize(*hInst);
    if(err != 0){
        Log3("audio unit initialize failed:[%d].",err);
        goto jumperr;
    }
    
    err = AudioOutputUnitStart(*hInst);
    if(err != 0){
        Log3("audio unit start failed:[%d].",err);
        goto jumperr;
    }
    
    return 0;

jumperr:
    free(p->hAUInst);
    return -1;
}

static int audioUnitDestroyEngine(OPENXL_STREAM * p){
    Log3("close audio unit play and capture.");
    
    if(!p) return -1;
    
    AudioComponentInstance * hInst = (AudioComponentInstance *)p->hAUInst;
    AudioOutputUnitStop(*hInst);
    AudioComponentInstanceDispose(*hInst);
    
    free(p->hAUInst);
    free(p);
    
    return 0;
}

#endif

// open the android audio device for input and/or output
OPENXL_STREAM * InitOpenXLStream(
	int 	sr, 
	int 	ichannels, 
	int 	ochannels, 
	void * 	context,
	bqRecordCallback cbr,
    bqPlayerCallback cbp
){
    OPENXL_STREAM * p;
    p = (OPENXL_STREAM *) malloc(sizeof(OPENXL_STREAM));
    memset(p, 0, sizeof(OPENXL_STREAM));
    p->ichannels = ichannels;
    p->ochannels = ochannels;
    p->sr = sr;
    p->cbr = cbr;
    p->cbp = cbp;
    p->recordBuffer = NULL;
    p->outputBuffer = NULL;

#ifdef PLATFORM_ANDROID
    
    p->context = context;
    p->bqPlayerBufferQueue = NULL;
    p->recorderBufferQueue = NULL;
    
    // for opensl on android
    
    if(openSLCreateEngine(p) != SL_RESULT_SUCCESS) {
        Log2("open sl engine failed.");
        FreeOpenSLStream(p);
        goto jumperr;
    }

    if(openSLRecordOpen(p) != SL_RESULT_SUCCESS) {
        Log2("open sl record failed.");
        FreeOpenSLStream(p);
        goto jumperr;
    } 

    if(openSLPlayerOpen(p) != SL_RESULT_SUCCESS) {
        Log2("open sl player failed.");
        FreeOpenSLStream(p);
        goto jumperr;
    }
    
#else
    
    p->context = p;
    
    // for openal on ios
    if(audioUnitCreateEngine(p) != 0){
        Log2("open al engine failed.");
        goto jumperr;
    }
    
#endif

    p->time = 0.;
    return p;
    
jumperr:
    free(p); p = NULL;
    
    return NULL;
}

// close the android audio device
void FreeOpenXLStream(OPENXL_STREAM *p){

    if (p == NULL)
    return;

#ifdef PLATFORM_ANDROID
    openSLDestroyEngine(p);
#else
    audioUnitDestroyEngine(p);
#endif

    free(p);
}


