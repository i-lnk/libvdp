
#ifndef __FFMPEG_MP4_H_
#define __FFMPEG_MP4_H_

#include <sys/resource.h>

#ifdef PLATFORM_ANDROID
#include <sys/system_properties.h>
#include <sys/prctl.h>
#endif

int StartRecording(
	char * 		filename,
	int			fps,
	int			width,
	int			height,
	int			audio_samplerate,
	int  *  	audio_length
	);

int CloseRecording();

int WriteRecordingFrames(
	void * 		frameData,
	int 		frameLens,
	int			frameCode,
	int			frameType,
	long long 	ts
	);

int WriteFrameType();

#endif
