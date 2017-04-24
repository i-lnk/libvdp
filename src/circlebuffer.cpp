#include <stdio.h>
#include <stdlib.h>
#include "circlebuffer.h"

static inline unsigned int _max(unsigned int a, unsigned int b)
{
  return a > b ? a : b;
}

static inline unsigned int _min(unsigned int a, unsigned int b)
{
  return a > b ? b : a;
}

CCircleBuffer::CCircleBuffer( int Size )
{
	d = (char*)malloc( Size );
	size = Size;
	memset(d,0,Size);

	Clear();
}


CCircleBuffer::CCircleBuffer( int Count, int Audio10msLength )
{	
	d = (char*)malloc( Count * Audio10msLength );
	size = Count * Audio10msLength;
	memset(d,0,size);

	Clear();
}

CCircleBuffer::~CCircleBuffer()
{	
	if(d) free(d); d = NULL;
}

void CCircleBuffer::Clear(){
    wp = rp = 0;
}

int  CCircleBuffer::Available(){
	return size - (wp - rp);
}

unsigned int CCircleBuffer::Used(){
	return wp - rp;
}

unsigned int CCircleBuffer::Put(char * buffer, unsigned int len)
{
	unsigned int l;
	len = _min(len, size - wp + rp);
	/* first put the data starting from fifo->in to buffer end */
	l = _min(len, size - (wp & (size - 1)));
	memcpy(d + (wp & (size - 1)), buffer, l);
	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(d, buffer + l, len - l);
	wp += len;
	return len;
}
 
unsigned int CCircleBuffer::Get(char * buffer, unsigned int len)
{
	unsigned int l;  

	len = _min(len, wp - rp); 
	/* first get the data from fifo->out until the end of the buffer */
	l = _min(len, size - (rp & (size - 1))); 
	memcpy(buffer, d + (rp & (size - 1)), l); 
	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buffer + l, d, len - l); 
	rp += len; 
	return len; 
}

	

