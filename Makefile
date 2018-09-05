HOST=windows-x86_64
SET_SYSROOT=--sysroot=D:\Projects\src\android\SDK\android-toolchain-armv7a-17\sysroot
TOOLCHAIN=D:\Projects\src\android\SDK\android-toolchain-armv7a-17
CC=$(TOOLCHAIN)\bin\arm-linux-androideabi-gcc $(SET_SYSROOT)
LD=$(TOOLCHAIN)\bin\arm-linux-androideabi-ld  $(SET_SYSROOT)

CYGWIN_BASE = D:\tools\cygwin64
INCLUDE_PATH += $(CYGWIN_BASE)\work\jni\libvdp\include
LIBRARY_PATH += $(CYGWIN_BASE)\work\jni\libvdp\lib\android 

ANDROID_CFLAGS = -O2 -fPIC \
	-DX264_VERSION -DANDROID -DPLATFORM_ANDROID -DHAVE_PTHREAD \
	-fpermissive \
	-I$(INCLUDE_PATH) \
	-I$(INCLUDE_PATH)\ffmpeg \
	-I$(INCLUDE_PATH)\TUTK \
	-I$(INCLUDE_PATH)\opus \
	-I$(INCLUDE_PATH)\webrtc_aec \
	-I$(TOOLCHAIN)\sysroot\usr\include

LDFLAGS = -L$(LIBRARY_PATH) \
	-lm -llog -g -lc -landroid -lstdc++ \
	-shared -lGLESv2 -lEGL -llog -lOpenSLES -landroid -fPIC \
	-lavformat -lavcodec -lswscale -lavutil -lswresample -lpostproc \
	-ldl \
	$(TOOLCHAIN)\lib\gcc\arm-linux-androideabi\4.9.x\armv7-a\libgcc.a

OBJECT_FILE = src/circlebuffer.o \
src/H264Decoder.o \
src/PPPPChannel.o \
src/PPPPChannelManagement.o \
src/SearchDVS.o \
src/muxing.o \
src/openxl_io.o \
src/audio_codec_adpcm.o \
src/audio_codec_g711.o \
src/audio_codec_dec.o \
src/audio_codec_enc.o \
src/audio_codec_ext.o \
src/appreq.o \
src/apprsp.o \
src/libvdp.o

%.o:%.cpp
	$(CROSS)$(CC) $(ANDROID_CFLAGS) -c $< -o $@
	
%.o:%.c
	$(CROSS)$(CC) $(ANDROID_CFLAGS) -c $< -o $@
 
libvdp.so: $(OBJECT_FILE)
	$(CROSS)$(LD) -o libvdp.so $(OBJECT_FILE) $(LDFLAGS)

 
# Clean by deleting all the objs and the lib
clean:
	rm -f src/*.o libvdp.so
	
