#ifndef _AUDIO_DATALIST_H_
#define _AUDIO_DATALIST_H_

class CCircleBuffer
{
public:
	 CCircleBuffer(int Size);
	 CCircleBuffer(int Count,int Audio10msLength);
	~CCircleBuffer();

	void Clear();
	int Available();
	unsigned int Used();
	unsigned int Get(char* buffer, unsigned int len);
	unsigned int Put(char* buffer, unsigned int len);

protected:
   	int size;
	char * d;
	unsigned int wp;
	unsigned int rp;
};


#endif
