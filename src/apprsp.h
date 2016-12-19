//
//  apprsp.hpp
//  libvdp
//
//  Created by fork on 16/12/15.
//  Copyright © 2016年 fork. All rights reserved.
//

#ifndef _APP_RSP_H_
#define _APP_RSP_H_

#include <stdio.h>

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

typedef int (*CMD_RP)(
    int             MsgType,
    void *          Msg,
    char *          JsonBuffer,
    int             JsonBufferSize
);

typedef struct {
    int             CmdType;
    CMD_RP          RspJson;
}APP_CMD_RESP;

int Rsp2Json(
    int             Cmd,
    void *          Msg,
    char *          JsonBuffer,
    int             JsonBufferSize
);

#endif /* apprsp_hpp */
