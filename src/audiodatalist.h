#ifndef _AUDIO_DATALIST_H_
#define _AUDIO_DATALIST_H_

typedef struct AudioData_ {
    short * buf;
	unsigned long time;
	int    state;
	struct AudioData_ *next;
	struct AudioData_ *front;      

}AudioData;

class CAudioDataList
{
public:
	CAudioDataList(int count,int Audio10msLength);
	~CAudioDataList();  
    void Write(short *data,unsigned long time);
	int  CheckData();
	AudioData * Read();
protected:
    COMMO_LOCK	m_Lock;
	short *		m_buf;
    int 		m_count;
	AudioData *	m_head;
	AudioData *	m_read;
	AudioData *	m_write;
private:
	int 		m_frame_lens;
};


#endif
