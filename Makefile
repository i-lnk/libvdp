## Host system
HOST = windows-x86_64
NDK = /work/android-ndk-r10b

## Android root directory
ANDROID_BASE = /work/droid

ARM_PLATFORM = $(NDK)/platforms/android-9/arch-arm/
ARM_PREBUILT = $(NDK)/toolchains/arm-linux-androideabi-4.8/prebuilt/$(HOST)

## Android lib directory
ANDROID_LIBPATH = $(ANDROID_BASE)/lib
GCC_VERSION = 4.9

CYGWIN_BASE = D:\tools\cygwin64
INCPATH += $(CYGWIN_BASE)\work\jni\libvdp\include
LIBPATH += $(CYGWIN_BASE)\work\jni\libvdp\lib\android
SYSROOT  = $(CYGWIN_BASE)\$(ARM_PLATFORM)
SOURCES += $(CYGWIN_BASE)\$(NDK)\sources
TOOLCHAIN = $(CYGWIN_BASE)\$(ARM_PREBUILT)

ANDROID = android-9

# pthreads forced define for inline stuff from threads.h
CFLAGS_EXTRA += 

CROSS = $(ARM_PREBUILT)/bin/arm-linux-androideabi-
CC = g++ --sysroot=$(SYSROOT)

ANDROID_CFLAGS = -msoft-float -fPIC -mthumb-interwork \
	-DX264_VERSION -DANDROID -DPLATFORM_ANDROID -DHAVE_PTHREAD \
	-I$(CYGWIN_BASE)/$(ARM_PLATFORM)/usr/include \
	-I$(INCPATH)/ffmpeg \
	-I$(INCPATH)/opus \
	-I$(INCPATH)/TUTK \
	-I$(INCPATH)/webrtc_aec

# Android link flags 
ANDROID_LDFLAGS +=  -shared -nostdlib -Bsymbolic -lGLESv2 -llog -fPIC
FFMPEG_STATIC_LIB += $(LIBPATH)\libavformat.a \
	$(LIBPATH)\libavcodec.a \
	$(LIBPATH)\libavutil.a \
	$(LIBPATH)\libswresample.a \
	$(LIBPATH)\libswscale.a	\
	$(LIBPATH)\libx264.a 

ANDROID_LIBS = -L$(SYSROOT)\usr\lib \
	-lm -llog -g -lc -lstdc++ -landroid \
	$(SYSROOT)\usr\lib\crtbegin_so.o \
	$(SYSROOT)\usr\lib\crtend_so.o \
	$(SOURCES)\cxx-stl\gnu-libstdc++\4.8\libs\armeabi-v7a\libsupc++.a \
	$(SOURCES)\cxx-stl\gnu-libstdc++\4.8\libs\armeabi-v7a\libgnustl_static.a \
	$(TOOLCHAIN)\lib\gcc\arm-linux-androideabi\4.8\armv7-a\libgcc.a
	

OBJECT_FILE = src/CircleBuf.o \
src/ffmpeg_mp4.o \
src/H264Decoder.o \
src/global_h264decoder.o \
src/PPPPChannel.o \
src/PPPPChannelManagement.o \
src/SearchDVS.o \
src/openxl_io.o \
src/audio_codec_adpcm.o \
src/audio_codec_g711.o \
src/audio_codec_dec.o \
src/audio_codec_enc.o \
src/appreq.o \
src/apprsp.o \
src/audiodatalist.o \
src/object_jni.o

%.o:%.cpp
	$(CROSS)$(CC) $(ANDROID_CFLAGS) -c $< -o $@
	
%.o:%.c
	$(CROSS)$(CC) $(ANDROID_CFLAGS) -c $< -o $@
 
libvdp.so: $(OBJECT_FILE)
	$(CROSS)ld -o libvdp.so $(OBJECT_FILE) -lm -lz -ldl $(FFMPEG_STATIC_LIB) $(ANDROID_LIBS) $(ANDROID_LDFLAGS)

 
# Clean by deleting all the objs and the lib
clean:
	rm -f src/*.o libvdp.so
	
