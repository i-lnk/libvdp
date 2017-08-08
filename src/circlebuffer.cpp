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

CCircleBuffer::CCircleBuffer( int Size)
{
	d = (char*)malloc(Size);
	size = Size;
	memset(d,0,Size);
	lock_used = 0;
	dmsg = 0;

	Clear();
}

CCircleBuffer::CCircleBuffer( int Size, int Lock)
{
	lock_used = 0;
	dmsg = 0;

	if(Lock){
		INT_LOCK(&lock);
		lock_used = 1;
	}

	d = (char*)malloc(Size);
	size = Size;
	memset(d,0,Size);

	Clear();
}

CCircleBuffer::CCircleBuffer( int Count, int Audio10msLength, int Lock)
{	
	lock_used = 0;
	dmsg = 0;

	if(Lock){
		INT_LOCK(&lock);
		lock_used = 1;
	}

	d = (char*)malloc( Count * Audio10msLength );
	size = Count * Audio10msLength;
	memset(d,0,size);

	Clear();
}

CCircleBuffer::~CCircleBuffer()
{	
	if(d){
		free(d); d = NULL;
	}
	
	if(lock_used){
		DEL_LOCK(&lock);
	}
}

void CCircleBuffer::GetLock(){
	if(lock_used) GET_LOCK(&lock);
}

void CCircleBuffer::PutLock(){
	if(lock_used) PUT_LOCK(&lock);
}

void CCircleBuffer::Debug(int val){
	dmsg = val;
}

void CCircleBuffer::Clear(){
	GetLock();
    wp = rp = 0;
	PutLock();
}

int  CCircleBuffer::Available(){
	GetLock();
	unsigned int val = size - (wp - rp);
	PutLock();
	return val;
}

unsigned int CCircleBuffer::Used(){
	GetLock();
	unsigned int val = wp - rp;
	PutLock();
	return val;
}

unsigned int CCircleBuffer::Put(char * buffer, unsigned int len)
{
	unsigned int l;

	GetLock();
	len = _min(len, size - wp + rp);
	/* first put the data starting from fifo->in to buffer end */
	l = _min(len, size - (wp & (size - 1)));
	memcpy(d + (wp & (size - 1)), buffer, l);
	/* then put the rest (if any) at the beginning of the buffer */
	memcpy(d, buffer + l, len - l);
	wp += len;
	PutLock();
	
	return len;
}

unsigned int CCircleBuffer::Mov(unsigned int len){
	unsigned int l;  

	GetLock();
	len = _min(len, wp - rp); 
	l = _min(len, size - (rp & (size - 1))); 

	rp += len;
	PutLock();

	return len;
}
 
unsigned int CCircleBuffer::Get(char * buffer, unsigned int len)
{
	unsigned int l;  

	GetLock();
	len = _min(len, wp - rp); 
	/* first get the data from fifo->out until the end of the buffer */
	l = _min(len, size - (rp & (size - 1))); 

#if 0
	if(dmsg){
		Log3("1.memcpy to [%p] from [%p] lens [%d]",
			buffer,d + (rp & (size - 1)),l
			);
		Log3("2.memcpy to [%p] from [%p] lens [%d]",
			buffer + l,d,len - l
			);
	}
#endif
	
	memcpy(buffer, d + (rp & (size - 1)), l); 
	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buffer + l, d, len - l); 
	rp += len;
	PutLock();
	
	return len; 
}

	

