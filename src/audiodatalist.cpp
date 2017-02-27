#include "utility.h"
#include "audiodatalist.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
	
CAudioDataList::CAudioDataList( int count, int Audio10msLength )
{
	Log2( "CEcrefList::CEcrefList" );
	m_head	= NULL;
	m_count = count;
	AudioData * data	= NULL;
	AudioData * front	= NULL;
	char * buf = NULL;
	int size = Audio10msLength;
	
	m_buf = (short *) malloc( count * size );
	m_frame_lens = size;
	
	buf	= (char*)m_buf;

	INT_LOCK( &m_Lock );

	for ( int i = 0; i < count; i++ )
	{
		data = (AudioData *) malloc( sizeof(AudioData) );
		if ( data ){
			/* memset(data,0,sizeof(AudioData)); */
			data->next	= NULL;
			data->front	= NULL;
			data->buf = (short *)buf;
			buf	+= size;
			data->time	=  0;
            data->state	= -1;
			data->front	= front;

			if ( front != NULL ){
				front->next = data;
			}
		}

		if ( i == 0 ){
			m_head = data;
		}else if ( i == count - 1 ){
			data->next	= m_head;
			m_head->front	= data;
		}
		front = data;
	}

	m_read	= m_head;
	m_write = m_head;
	m_count = count;
}


CAudioDataList::~CAudioDataList()
{
/*   Log2("CEcrefList::ReleaseAll"); */
	if ( m_head != NULL && m_head->front != NULL )
	{
		m_head->front->next = NULL;
	}
	AudioData	*data	= m_head;
	AudioData	*next	= NULL;
	for ( int i = 0; i < m_count; i++ )
	{
		if ( data != NULL )
		{
			/* Log2("CEcrefList::fdafdas  %d",i); */
			next = data->next;
			free( data );
			data = next;
		}
	}

	if ( m_buf )
	{
		free( m_buf );
		m_buf = NULL;
	}
/*  Log2("CEcrefList::ReleaseAll end"); */

	DEL_LOCK( &m_Lock );
}


void CAudioDataList::Write(short * data,unsigned long time)
{
    GET_LOCK(&m_Lock);
	
//    double mean;
    int size = m_frame_lens/sizeof(short);

    if(m_write!= NULL){
		for(int i = 0;i<size;i++){
            m_write->buf[i] = data[i] > 32767 ? 32767 : data[i];
		}
		m_write->time = time;
		m_write->state = 1;
		m_write = m_write->next;   
    }
	
	PUT_LOCK(&m_Lock);
}

int  CAudioDataList::CheckData()
{
	GET_LOCK(&m_Lock);
	if(m_read!= NULL){
	   PUT_LOCK(&m_Lock);
	   return m_read->state;
	}
	PUT_LOCK(&m_Lock);
	return 0;
}

AudioData* CAudioDataList::Read()
{
	GET_LOCK(&m_Lock);
	AudioData * data = NULL;
	if(m_read!= NULL &&  m_read->state == 1){
        data = m_read;
	    m_read->state = 0;
	    m_read = data->next;
	}
	PUT_LOCK(&m_Lock);
	return data;
}
	

