#ifndef _CIRCLEBUF_H_
#define _CIRCLEBUF_H_

#include "utility.h"

//视频缓存中，每包视频数据的头部
struct VIDEO_BUF_HEAD
{
    unsigned int	head; 		// 头，必须等于0xFF00FF 
    unsigned int	timestamp; 	// 时间戳，如果是录像，则填录像的时间戳，如果是实时视频，则为0
    unsigned int 	len;    	// 长度
    unsigned int 	frametype;
	unsigned int 	streamid;
	unsigned long 	time;
	char            d[0];
};

class CCircleBuf
{    
public:
	CCircleBuf();
	~CCircleBuf();

	bool Create( int size );


	void Release();


	int Read( void* buf, int size );


	int Write( void* buf, int size );


	int GetStock();


	void Reset();


	void AllRest();


	/* 这个函数用来特殊用处 */
	char * ReadOneFrame( int &len );


	char * ReadOneFrame1( int &len, VIDEO_BUF_HEAD & videobufhead );


	char * ReadOneFrameX( char * pData, int DataSize );


private:
	int Read1( void* buf, int size );


protected:
	char	* m_pBuf;
	int	m_nSize;
	int	m_nStock;
	int	m_nReadPos;
	int	m_nWritePos;

	int m_nTimeout;

	COMMO_LOCK Lock;

private:

	int m_n;
};


#endif

