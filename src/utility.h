/**
 * Copyright (C) 2011, Skype Limited
 *
 * All intellectual property rights, including but not limited to copyrights,
 * trademarks and patents, as well as know how and trade secrets contained in,
 * relating to, or arising from the internet telephony software of
 * Skype Limited (including its affiliates, "Skype"), including without
 * limitation this source code, Skype API and related material of such
 * software proprietary to Skype and/or its licensors ("IP Rights") are and
 * shall remain the exclusive property of Skype and/or its licensors.
 * The recipient hereby acknowledges and agrees that any unauthorized use of
 * the IP Rights is a violation of intellectual property laws.
 *
 * Skype reserves all rights and may take legal action against infringers of
 * IP Rights.
 *
 * The recipient agrees not to remove, obscure, make illegible or alter any
 * notices or indications of the IP Rights and/or Skype's rights and
 * ownership thereof.
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <string.h>
#include <pthread.h>

#ifdef PLATFORM_ANDROID
#include "android/log.h"

// Templated array length
template<typename T>
size_t lenof_x(T &x) {
    return (sizeof(x) / sizeof(*x));
}

// Templated min()
template<typename T>
T min(const T &a, const T &b) { return a > b ? b : a ;}
#else

#endif

typedef pthread_mutex_t	COMMO_LOCK,*PCOMMO_LOCK;
typedef	pthread_t		PROCESS_HANDLE;

#define GET_LOCK(X) pthread_mutex_lock(X)
#define PUT_LOCK(X) pthread_mutex_unlock(X)
#define TRY_LOCK(X) pthread_mutex_trylock(X)

#define INT_LOCK(X) pthread_mutex_init(X,NULL);
#define DEL_LOCK(X) pthread_mutex_destroy(X);

// Macro to prevent obj copies
#define NOCOPY(T)				\
	private:                    \
	T(const T &);				\
	T &operator=(const T &);


/*
 *  Logging
 *
 */
#ifndef LOG_TAG
#define LOG_TAG "utilityCode"
#endif

#ifdef PLATFORM_ANDROID
#define Log(...)
#define Log2(fmt, args...)
#define Log3(fmt, args...) \
	{	\
		__android_log_print(ANDROID_LOG_ERROR, "P2PLIB","= %-16s, line %4d, %-16s:"fmt"\n", __FILE__, __LINE__, __FUNCTION__, ##args); \
	}

#define F_LOG LogBlock _l(__func__)
#else
#define Log(...)
#define Log2(fmt, ...)
//#define Log3(fmt, ...) NSLog((@"P2PLIB FUNC:[%s],LINE:[%s]:" @fmt @"\n"),__PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__);
#define Log3(fmt, args...)
#define LogX(fmt, args...) \
    fprintf(stderr,"P2PLIB FILE:[%s],LINE:[%4d]:" fmt "\n",strrchr(__FILE__,'/'), __LINE__, ##args);
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)\
{\
	if((p) != NULL)\
	{\
		delete (p) ;\
		(p) = NULL ;\
	}\
}
#endif

#endif /* UTILITY_H_ */
