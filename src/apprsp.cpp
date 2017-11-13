//
//  apprsp.cpp
//  libvdp
//
//  Created by fork on 16/12/15.
//  Copyright © 2016年 fork. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "apprsp.h"

// function for device response

static int SetUUID(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlSetUUIDResp * hRQ = (SMsgAVIoctrlSetUUIDResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%s\"}",
            "result",hRQ->result,
            "uuid",hRQ->uuid
            );
    
    return 0;
}

static int SetPassword(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlSetPasswdResp * hRQ = (SMsgAVIoctrlSetPasswdResp *)Msg;
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "result",hRQ->result,
            "none",0
            );
    
    return 0;
}

static int SetPush(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }

	SMsgAVIoctrlSetPushResp * hRQ = (SMsgAVIoctrlSetPushResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "result",hRQ->result,
            "none",0
            );
    
    return 0;
}

static int DelPush(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }

	SMsgAVIoctrlDelPushResp * hRQ = (SMsgAVIoctrlDelPushResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "result",hRQ->result,
            "none",0
            );
    
    return 0;
}

static int GetAP(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlListWifiApResp * hRQ = (SMsgAVIoctrlListWifiApResp *)Msg;
    int i = 0;
    int lens = 0;
    
    if(hRQ->number > 12) hRQ->number = 12;
    
    lens += sprintf(JsonBuffer,"[");
    for(i=0;i<(int)hRQ->number;i++){
        
        if(strlen(hRQ->stWifiAp[i].ssid) == 0){
            continue;
        }
        
        lens += sprintf(JsonBuffer + lens,"{\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"},",
                        "ssid",hRQ->stWifiAp[i].ssid,
                        "encrypt",hRQ->stWifiAp[i].enctype,
                        "mode",hRQ->stWifiAp[i].mode,
                        "signal",hRQ->stWifiAp[i].signal,
                        "status",hRQ->stWifiAp[i].status
                        );
    }
    lens += sprintf(JsonBuffer + lens,"]");
    
    return 0;
}

static int GetWireless(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetWifiResp2 * hRQ = (SMsgAVIoctrlGetWifiResp2 *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "ssid",hRQ->ssid,
            "password",hRQ->password,
            "encrypt",hRQ->enctype,
            "mode",hRQ->mode,
            "signal",hRQ->signal,
            "status",hRQ->status
            );
    
    return 0;
}

static int GetRecordSchedule(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetRecordResp * hRQ = (SMsgAVIoctrlGetRecordResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "channel",hRQ->channel,
            "type",hRQ->recordType,
            "start_hour",hRQ->startHour,
            "start_mins",hRQ->startMins,
            "close_hour",hRQ->closeHour,
            "close_mins",hRQ->closeMins,
            "video_lens",hRQ->videoLens
            );
    
    return 0;
}

static int GetMotionSchedule(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetMDPResp * hRQ = (SMsgAVIoctrlGetMDPResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "enable",hRQ->MotionEnable,
            "level",hRQ->MotionLevel,
            "frequency",hRQ->MotionAlarmFrequency,
            "type",hRQ->MotionAlarmType,
            "start_hour",hRQ->MotionStartHour,
            "start_mins",hRQ->MotionStartMins,
            "close_hour",hRQ->MotionCloseHour,
            "close_mins",hRQ->MotionCloseMins
            );
    
    return 0;
}

static int GetMotionScheduleEx(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlMDAlarmResp * hRQ = (SMsgAVIoctrlMDAlarmResp *)Msg;
    
    sprintf(JsonBuffer,"{"
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"}",
        "enable",hRQ->MotionEnable,
        "level",hRQ->MotionLevel,
        "frequency",hRQ->MotionAlarmFrequency,
        "notify",hRQ->MotionNotify,
        "record",hRQ->MotionRecord,
        "audio",hRQ->MotionAudioOutput,
        "enablePIR",hRQ->EnablePir,
        "enableRemoveAlaram",hRQ->EnableRemoveAlarm,
        "start_hour",hRQ->MotionStartHour,
        "start_mins",hRQ->MotionStartMins,
        "close_hour",hRQ->MotionCloseHour,
        "close_mins",hRQ->MotionCloseMins
        );
    
    return 0;
}

static int GetDeviceAlarming(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlAlarmingReq * hRQ = (SMsgAVIoctrlAlarmingReq *)Msg;
    
    char sTIME[64] = {0};
    char sTYPE[16] = {0};
    
    const char * wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm * p;
    
    sprintf(sTYPE,"%d",hRQ->AlarmType);
    p = localtime((const time_t *)&hRQ->AlarmTime); //
    sprintf(sTIME,
            "%04d-%02d-%02d-%s-%02d-%02d-%02d",
            1900+p->tm_year,
            p->tm_mon + 1,
            p->tm_mday,
            wday[p->tm_wday],
            p->tm_hour,
            p->tm_min,
            p->tm_sec);
    
    sprintf(JsonBuffer,"{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}",
            "DID",hRQ->AlarmDID,
            "type",sTYPE,
            "time",sTIME
            );
    
    return 0;
}

static int GetVideoMode(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetVideoModeResp * hRQ = (SMsgAVIoctrlGetVideoModeResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "channel",hRQ->channel,
            "flip",hRQ->mode & AVIOCTRL_VIDEOMODE_FLIP,
            "mirrow",hRQ->mode & AVIOCTRL_VIDEOMODE_MIRROR
            );
    
    return 0;
}

static int GetStreamCtrl(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetStreamCtrlResp * hRQ = (SMsgAVIoctrlGetStreamCtrlResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "channel",hRQ->channel,
            "quality",hRQ->quality
            );
    
    return 0;
}

static int GetSystem(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetSystemResp * hRQ = (SMsgAVIoctrlGetSystemResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "language",hRQ->lang,
            "datetime",hRQ->datetime,
            "enableAutomicUpdate",hRQ->enableAutomicUpdate,
            "enablePreviewUnlock",hRQ->enablePreviewUnlock,
            "enableRingingButton",hRQ->enableRingingButton
            );
    
    return 0;
}

static int GetTimezone(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlTimeZone * hRQ = (SMsgAVIoctrlTimeZone *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\"}","timezone",hRQ->nTimeZone);
    
    return 0;
}

static int GetSDCard(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetSDCardResp * hRQ = (SMsgAVIoctrlGetSDCardResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\"}",
            "status",hRQ->status,
            "size",hRQ->size,
            "free",hRQ->free,
            "progress",hRQ->progress
            );
    
    return 0;
}

static int GetFormatExtStorage(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlFormatExtStorageResp * hRQ = (SMsgAVIoctrlFormatExtStorageResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "index",hRQ->storage,
            "result",hRQ->result
            );
    
    return 0;
}

static int GetOSD(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetOSDResp * hRQ = (SMsgAVIoctrlGetOSDResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%s\"}",
            "channel",hRQ->channel,
            "channel_name_text",strlen(hRQ->channel_name_text) >= sizeof(hRQ->channel_name_text) ? "invalid text" : hRQ->channel_name_text
            );
    
    return 0;
}

static int SetOSD(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlSetOSDResp * hRQ = (SMsgAVIoctrlSetOSDResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\"}","result",hRQ->result);
    
    return 0;
}

static int ParingRFDev(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlParingRFResp * hRQ = (SMsgAVIoctrlParingRFResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\"}","result",hRQ->result);
    
    return 0;
}

static int SelectRFDev(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlSelectRFResp * hRQ = (SMsgAVIoctrlSelectRFResp *)Msg;
    
    int i = 0;
    int lens = 0;
    
    lens += sprintf(JsonBuffer,"[");
    for(i=0;i<(int)hRQ->num;i++){
        
        lens += sprintf(JsonBuffer + lens,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\"},",
                        "id",hRQ->dev[i].id,
                        "code",hRQ->dev[i].code,
                        "type",hRQ->dev[i].type,
                        "name",hRQ->dev[i].name
                        );
    }
    lens += sprintf(JsonBuffer + lens,"]");
    
    return 0;
}

static int GetUpdate(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }

    SMsgAVIoctrlUpdateResp * hRQ = (SMsgAVIoctrlUpdateResp *)Msg;

    sprintf(JsonBuffer,"{\"%s\":\"%d\"}","result",0);

    return 0;
}

static int GetUpdateProgress(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlUpdateProgResp * hRQ = (SMsgAVIoctrlUpdateProgResp *)Msg;
    
    sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\"}",
            "status",hRQ->status,
            "result",hRQ->progress
            );
    
    return 0;
}

static int GetCapacity(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetCapacityResp * hRQ = (SMsgAVIoctrlGetCapacityResp *)Msg;
    
    sprintf(JsonBuffer,"{"
		"\"%s\":\"%d\",\"%s\":\"%s\",\"%s\":\"%d\","
		"\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\""
		"}",
        "devType",hRQ->devType,
        "version",hRQ->version,
        "odm",hRQ->odmID,
        "model",hRQ->model,
        "language",hRQ->language,
        "supportStorage",hRQ->supportStorage,
        "supportPTZ",hRQ->supportPTZ,
        "supportPIR",hRQ->supportPIR,
        "supportRemoveAlarm",hRQ->supportRemoveAlarm,
        "supportAudioIn",hRQ->supportAudioIn,
        "supportAudioOut",hRQ->supportAudioOut
        );
    
    return 0;
}

static int GetEventList(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlListEventResp * hRQ = (SMsgAVIoctrlListEventResp *)Msg;

	char * lps = JsonBuffer;
	int    len = 0;

	int count = hRQ->count;

	len += sprintf(lps + len,"{");
	len += sprintf(lps + len,"\"total\":\"%d\",\"count\":\"%d\",\"index\":\"%d\",\"end\":\"%d\",\"record\":[",
		hRQ->total,
		hRQ->count,
		hRQ->index,
		hRQ->endflag
		);

	Log3("%s}",lps);

	for(int i = 0;i < count;i++){
	    len += sprintf(lps + len,"{\"%s\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"%s\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"%s\":\"%d\",\"%s\":\"%d\"},",
	          	"start",
	          	hRQ->stEvent[i].startTime.year,hRQ->stEvent[i].startTime.month, hRQ->stEvent[i].startTime.day,
	          	hRQ->stEvent[i].startTime.hour,hRQ->stEvent[i].startTime.minute,hRQ->stEvent[i].startTime.second,
                "close",
                hRQ->stEvent[i].closeTime.year,hRQ->stEvent[i].closeTime.month, hRQ->stEvent[i].closeTime.day,
                hRQ->stEvent[i].closeTime.hour,hRQ->stEvent[i].closeTime.minute,hRQ->stEvent[i].closeTime.second,
	          	"event",hRQ->stEvent[i].event,
	          	"status",hRQ->stEvent[i].status
	            );
	}

	if(count != 0){
		len -= 1; // replace last charecter ","
	}
	len += sprintf(lps + len,"]}");
    
    return 0;
}

static int GetEventListByMonth(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlEventByMonthResp * hRQ = (SMsgAVIoctrlEventByMonthResp *)Msg;

	sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%ld\"}",
            "year",hRQ->year,
            "month",hRQ->month,
            "map",hRQ->map
            );
    
    return 0;
}

static int XMCallReq(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlCallReq * hRQ = (SMsgAVIoctrlCallReq *)Msg;

	sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%04d-%02d-%02d %02d:%02d:%02d\"}",
			"index",hRQ->index,
            "time",
	         hRQ->stTime.year,hRQ->stTime.month, hRQ->stTime.day,
	         hRQ->stTime.hour,hRQ->stTime.minute,hRQ->stTime.second
            );
    
    return 0;
}

static int XMCallInd(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlCallInd * hRQ = (SMsgAVIoctrlCallInd *)Msg;

	sprintf(JsonBuffer,"{\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%04d-%02d-%02d %02d:%02d:%02d\"}",
			"index",hRQ->index,
			"type",hRQ->type,
            "time",
	         hRQ->stTime.year,hRQ->stTime.month, hRQ->stTime.day,
	         hRQ->stTime.hour,hRQ->stTime.minute,hRQ->stTime.second
            );
    
    return 0;
}

static int GetBatteryStatus(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetBatteryResp * hRQ = (SMsgAVIoctrlGetBatteryResp *)Msg;

	sprintf(JsonBuffer,"{\"%s\":\"%d\"}","battery",hRQ->battery);
    
    return 0;
}

static int GetCameraView(
int             Cmd,
void *          Msg,
char *          JsonBuffer,
int             JsonBufferSize
){
    if(Msg == NULL || JsonBuffer == NULL){
        return -1;
    }
    
    SMsgAVIoctrlGetCameraViewResp * hRQ = (SMsgAVIoctrlGetCameraViewResp *)Msg;

	sprintf(JsonBuffer,"{\"%s\":\"%d\"}","index",hRQ->index);
    
    return 0;
}

static APP_CMD_RESP hACR[] = {
{IOTYPE_USER_IPCAM_SET_UUID,SetUUID},
{IOTYPE_USER_IPCAM_SETPASSWORD_RESP,SetPassword},
{IOTYPE_USER_IPCAM_SET_PUSH_RESP,SetPush},
{IOTYPE_USER_IPCAM_DEL_PUSH_RESP,DelPush},
{IOTYPE_USER_IPCAM_LISTWIFIAP_RESP,GetAP},
{IOTYPE_USER_IPCAM_GETWIFI_RESP,GetWireless},
{IOTYPE_USER_IPCAM_GETWIFI_RESP_2,GetWireless},
{IOTYPE_USER_IPCAM_SETRECORD_RESP,GetRecordSchedule},
{IOTYPE_USER_IPCAM_GETRECORD_RESP,GetRecordSchedule},
{IOTYPE_USER_IPCAM_SETMOTIONDETECT_RESP,GetMotionSchedule},
{IOTYPE_USER_IPCAM_GETMOTIONDETECT_RESP,GetMotionSchedule},
{IOTYPE_USER_IPCAM_GET_MD_ALAM_RESP,GetMotionScheduleEx},
{IOTYPE_USER_IPCAM_SET_MD_ALAM_RESP,GetMotionScheduleEx},
{IOTYPE_USER_IPCAM_SET_MDP_RESP,GetMotionSchedule},
{IOTYPE_USER_IPCAM_GET_MDP_RESP,GetMotionSchedule},
{IOTYPE_USER_IPCAM_ALARMING_REQ,GetDeviceAlarming},
{IOTYPE_USER_IPCAM_GET_VIDEOMODE_RESP,GetVideoMode},
{IOTYPE_USER_IPCAM_GETSTREAMCTRL_RESP,GetStreamCtrl},
{IOTYPE_USER_IPCAM_SET_SYSTEM_RESP,GetSystem},
{IOTYPE_USER_IPCAM_GET_SYSTEM_RESP,GetSystem},
{IOTYPE_USER_IPCAM_GET_TIMEZONE_RESP,GetTimezone},
{IOTYPE_USER_IPCAM_SET_TIMEZONE_RESP,GetTimezone},
{IOTYPE_USER_IPCAM_GET_SDCARD_RESP,GetSDCard},
{IOTYPE_USER_IPCAM_FORMATEXTSTORAGE_RESP,GetFormatExtStorage},
{IOTYPE_USER_IPCAM_GET_OSD_RESP,GetOSD},
{IOTYPE_USER_IPCAM_SET_OSD_RESP,SetOSD},
{IOTYPE_USER_IPCAM_PARING_RF_RESP,ParingRFDev},
{IOTYPE_USER_IPCAM_REMOVE_RF_RESP,ParingRFDev},
{IOTYPE_USER_IPCAM_CONFIG_RF_RESP,ParingRFDev},
{IOTYPE_USER_IPCAM_PARING_RF_EXIT_RESP,ParingRFDev},
{IOTYPE_USER_IPCAM_SELECT_RF_RESP,SelectRFDev},
{IOTYPE_USER_IPCAM_UPDATE_RESP,GetUpdate},
{IOTYPE_USER_IPCAM_UPDATE_PROG_RESP,GetUpdateProgress},
{IOTYPE_USER_IPCAM_GET_CAPACITY_RESP,GetCapacity},
{IOTYPE_USER_IPCAM_LISTEVENT_RESP,GetEventList},
{IOTYPE_USER_IPCAM_LISTEVENT_BY_MONTH_RESP,GetEventListByMonth},
{IOTYPE_XM_CALL_REQ,XMCallReq},
{IOTYPE_XM_CALL_IND,XMCallInd},
{IOTYPE_USER_IPCAM_GET_BATTERY_RESP,GetBatteryStatus},
{IOTYPE_USER_IPCAM_GET_CAMERA_VIEW_RESP,GetCameraView},
{0,NULL}
};

int ParseResponseForUI(
    int             Cmd,
    void *          Msg,
    char *          JsonBuffer,
    int             JsonBufferSize
){
    //  Log3("CCCCCCCCCCCCall RecvCmds:[%08X].\n",Cmd);
    
    int i = 0;
    for(i = 0;hACR[i].RspJson != NULL;i++){
        if(Cmd == hACR[i].CmdType){
            
            Log3("app cmd recv IOCTRL cmd:[%03X].",hACR[i].CmdType);
            
            return hACR[i].RspJson(
                       Cmd,
                       Msg,
                       JsonBuffer,
                       JsonBufferSize);
        }
    }
    
    return -1;
}
