#ifndef _APP_REQ_H_
#define _APP_REQ_H_

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

typedef int (*CMD_DO)(
	int				Idx,
	int				MsgType,
	const char *	Cgi,
	int				CgiLens,
	void *			Params
);

typedef struct {
	int		CmdType;
	CMD_DO	CmdCall;
}APP_CMD_CALL;

typedef struct {
    int     Magic;
	int		AppType;
	int		CgiLens;
	char	CgiData[0];
}APP_CMD_HEAD;

char * StringSafeCopyEx(
	char *			To,
	const char *	From,
	int				ToBufferSize,
	const char *	Begin,
	const char *	End
);

int SendCmds(
	int 			Idx,
	int				MsgType,
	const char *	Cgi,
	int				CgiLens,
	void *			Params
);

#endif
