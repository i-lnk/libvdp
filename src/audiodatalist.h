#ifndef _AUDIO_DATALIST_H_
#define _AUDIO_DATALIST_H_

#define DEFAULT_AUDIO_SIZE 256	
#define MIN_PCM_AUDIO_SIZE 160

typedef struct AudioData_ {
    short *buf;
	unsigned long time;
	int    state;
	struct AudioData_ *next;
	struct AudioData_ *front;      

}AudioData;

class CAudioDataList
{
public:
	CAudioDataList(int count);
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
	
};


#endif
