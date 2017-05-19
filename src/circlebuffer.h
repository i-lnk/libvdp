#ifndef _AUDIO_DATALIST_H_
#define _AUDIO_DATALIST_H_

#include "utility.h"

class CCircleBuffer
{
public:
	 CCircleBuffer(int Size);
	 CCircleBuffer(int Size,int Lock);
	 CCircleBuffer(int Count,int Audio10msLength,int Lock);
	~CCircleBuffer();

	void Clear();
	int Available();
	unsigned int Used();
	unsigned int Get(char* buffer, unsigned int len);
	unsigned int Put(char* buffer, unsigned int len);

protected:
   	int 			size;
	char * 			d;
	unsigned int  	wp;
	unsigned int  	rp;

	int				lock_used;
	COMMO_LOCK		lock;

private:
	void GetLock();
	void PutLock();
};


#endif
