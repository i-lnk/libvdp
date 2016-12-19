#include "utility.h"
#include "CircleBuf.h"


CCircleBuf::CCircleBuf()
{
	m_pBuf		= NULL;
	m_nSize		= 0;
	m_nStock	= 0;
	m_nWritePos	= 0;
	m_nReadPos	= 0;

	INT_LOCK( &Lock );
}


void CCircleBuf::AllRest()
{
	memset( m_pBuf, 0, sizeof(m_pBuf) );
	m_nSize		= 0;
	m_nStock	= 0;
	m_nWritePos	= 0;
	m_nReadPos	= 0;
}


CCircleBuf::~CCircleBuf()
{
	Release();

	DEL_LOCK( &Lock );
}


bool CCircleBuf::Create( int size )
{
	GET_LOCK( &Lock );

	if ( size <= 0 )
		return(false);

	if ( m_pBuf != NULL )
	{
		delete m_pBuf;
		m_pBuf = NULL;
	}


	m_pBuf = new char[size];
	if ( m_pBuf == NULL ){
		PUT_LOCK( &Lock );
		return(false);
	}

	m_nSize		= size;
	m_nStock	= 0;
	m_nWritePos	= 0;
	m_nReadPos	= 0;

	PUT_LOCK( &Lock );

	return(true);
}


void CCircleBuf::Release()
{
	/* F_LOG; */

	GET_LOCK( &Lock );


	if ( m_pBuf == NULL )
	{
		PUT_LOCK( &Lock );
		return;
	}

	if ( m_pBuf != NULL )
	{
		delete m_pBuf;
		m_pBuf = NULL;
	}

	m_nSize		= 0;
	m_nStock	= 0;
	m_nReadPos	= 0;
	m_nWritePos	= 0;

	PUT_LOCK( &Lock );
}


char* CCircleBuf::ReadOneFrame1( int &len, VIDEO_BUF_HEAD & videobufhead )
{
	GET_LOCK( &Lock );


	len = 0;

	if ( m_nStock == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	char		*pbuf = NULL;
	VIDEO_BUF_HEAD	videohead;
	int		nRet = Read1( (char *) &videohead, sizeof(VIDEO_BUF_HEAD) );
	if ( nRet == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	pbuf	= new char[videohead.len];
	nRet	= Read1( (char *) pbuf, videohead.len );
	if ( nRet == 0 )
	{
		delete pbuf;
		PUT_LOCK( &Lock );
		return(NULL);
	}

	memcpy( (char *) &videobufhead, (char *) &videohead, sizeof(VIDEO_BUF_HEAD) );

	len = videohead.len;
	
	PUT_LOCK( &Lock );
	
	return(pbuf);
}


char* CCircleBuf::ReadOneFrameX( char * pData, int DataSize )
{
	GET_LOCK( &Lock );

	if ( m_nStock == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	/*   char *pbuf = pData; */

	VIDEO_BUF_HEAD * hVBH = (VIDEO_BUF_HEAD *) pData;

	int nRet = Read1( pData, sizeof(VIDEO_BUF_HEAD) );
	if ( nRet == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	nRet = Read1( hVBH->d, hVBH->len );
	if ( nRet == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	PUT_LOCK( &Lock );

	return(pData);
}


char* CCircleBuf::ReadOneFrame( int &len )
{
	GET_LOCK( &Lock );
		
	len = 0;

	if ( m_nStock == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	char		*pbuf = NULL;
	VIDEO_BUF_HEAD	videohead;
	int		nRet = Read1( (char *) &videohead, sizeof(VIDEO_BUF_HEAD) );
	if ( nRet == 0 )
	{
		PUT_LOCK( &Lock );
		return(NULL);
	}

	pbuf	= new char[videohead.len];
	nRet	= Read1( (char *) pbuf, videohead.len );
	if ( nRet == 0 )
	{
		delete pbuf;
		PUT_LOCK( &Lock );
		return(NULL);
	}

	len = videohead.len;
	
	PUT_LOCK( &Lock );
	
	return(pbuf);
}


int CCircleBuf::Read1( void* buf, int size )
{
	if ( m_nStock < size )
	{
		return(0);
	}

	int	left	= 0;
	int	offs	= m_nWritePos - m_nReadPos;
	if ( offs > 0 )
	{
		memcpy( buf, &m_pBuf[m_nReadPos], size );
		m_nReadPos += size;
	}else  {
		offs = m_nSize - m_nReadPos;
		if ( offs > size )
		{
			memcpy( buf, &m_pBuf[m_nReadPos], size );
			m_nReadPos += size;
		}else  {
			memcpy( buf, &m_pBuf[m_nReadPos], offs );
			left = size - offs;
			memcpy( &( (char *) buf)[offs], m_pBuf, left );
			m_nReadPos = left;
		}
	}

	m_nStock -= size;

	return(size);
}


/* 从缓存中读数据，当缓存数据不够读则读取失败，返回为0 */
int CCircleBuf::Read( void* buf, int size )
{
	/* Lock the buffer */
	GET_LOCK( &Lock );

	if ( m_nStock < size )
	{
		PUT_LOCK( &Lock );
		return(0);
	}

	int	left	= 0;
	int	offs	= m_nWritePos - m_nReadPos;
	if ( offs > 0 )
	{
		memcpy( buf, &m_pBuf[m_nReadPos], size );
		m_nReadPos += size;
	}else  {
		offs = m_nSize - m_nReadPos;
		if ( offs > size )
		{
			memcpy( buf, &m_pBuf[m_nReadPos], size );
			m_nReadPos += size;
		}else  {
			memcpy( buf, &m_pBuf[m_nReadPos], offs );
			left = size - offs;
			memcpy( &( (char *) buf)[offs], m_pBuf, left );
			m_nReadPos = left;
		}
	}

	m_nStock -= size;

	PUT_LOCK( &Lock );

	return(size);
}


/* 写缓存，当缓存满时，不写入，返回0 */
int CCircleBuf::Write( void* buf, int size )
{
	/* Lock the buffer */
	GET_LOCK( &Lock );

	/* the buffer is full */
	if ( m_nStock + size > m_nSize )
	{
/*		Log2("m_nStock:%d size:%d m_nSize:%d",m_nStock,size,m_nSize); */
		PUT_LOCK( &Lock );

		return(0);
	}

	int	left	= 0;
	int	offs	= m_nSize - m_nWritePos;
	if ( offs > size )
	{
		memcpy( &m_pBuf[m_nWritePos], buf, size );
		m_nWritePos += size;
	}else  {
		memcpy( &m_pBuf[m_nWritePos], buf, offs );
		left = size - offs;
		memcpy( m_pBuf, &( (char *) buf)[offs], left );
		m_nWritePos = left;
	}

	m_nStock += size;

	PUT_LOCK( &Lock );

	return(size);
}


int CCircleBuf::GetStock()
{
	GET_LOCK( &Lock );

	m_n = m_nStock;

	PUT_LOCK( &Lock );

	return(m_n);
}


void CCircleBuf::Reset()
{
	GET_LOCK( &Lock );

	m_nReadPos	= 0;
	m_nWritePos	= 0;
	m_nStock	= 0;

	PUT_LOCK( &Lock );
}


