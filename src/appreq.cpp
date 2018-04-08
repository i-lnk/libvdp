#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "appreq.h"

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

static char encoding_table[] = {
'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
'w', 'x', 'y', 'z', '0', '1', '2', '3',
'4', '5', '6', '7', '8', '9', '+', '/',
};

static char * decoding_table = NULL;

static void Base64DecodeTableInit() {
    static char table[256] = {0};
    decoding_table = table;
    
    int i = 0;
    for (i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}

static int Base64Decode(
	const char *data,
	char * output,
    int input_length,
    int output_size
) {
    if (decoding_table == NULL) Base64DecodeTableInit();

    if (input_length % 4 != 0) return -1;

    int output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') output_length--;
    if (data[input_length - 2] == '=') output_length--;

    unsigned char *decoded_data = (unsigned char *)output;
    if (decoded_data == NULL) return -1;
	if (output_length > output_size) return -1;

	unsigned int i = 0;
	unsigned int j = 0;

    for (i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return output_length;
}

//
//	string safe copy between string
//
char * GetCgiParam(
	char *			To,
	const char *	From,
	int				ToBufferSize,
	const char *	Begin,
	const char *	End
){
	if(To == NULL || From == NULL) return NULL;
	const char * lpBegin = NULL;
	const char * lpEnd = NULL;

	if(Begin != NULL) lpBegin = strstr(From,Begin);
	
	if(lpBegin == NULL){
		return NULL;
//		lpBegin = From;
	}else{
		lpBegin = lpBegin + strlen(Begin);
	}

	int CopyLens = 0;
	int	CopySize = 0;
	
	lpEnd = strstr(lpBegin,End);
	if(lpEnd == NULL){
		CopySize = strlen(lpBegin);
		CopyLens = ToBufferSize > CopySize ? CopySize : ToBufferSize;
	}else{
		CopySize = lpEnd - lpBegin;
		CopyLens = ToBufferSize > CopySize ? CopySize : ToBufferSize;
	}

	int DataLens = Base64Decode(lpBegin,To,CopyLens,ToBufferSize);
	if(DataLens < 0) return NULL;

//	Log3("[%s%s]",Begin,To);
	
//	memcpy(To,lpBegin,CopyLens);

	return To;
}

static int SetUUIDData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlSetUUIDReq sMsg;

	char szUUID[64] = {0};

	// public static int IOTYPE_USER_IPCAM_SET_UUID	= 0x9999;	//  ÷ª˙∂À∑¢ÀÕ…Ë÷√…Ë±∏UUIDµƒ«Î«Û
	
	// String msg = "setuuid.cgi?UUID=" + UUID + "&";
	// Avapi.SendCtrlCommand(ConnectService.device_id, Constant.IOTYPE_USER_IPCAM_SET_UUID, msg, msg.length());

	GetCgiParam((char*)szUUID,szCgi,sizeof(szUUID),"UUID=","&");

	memset(&sMsg,0,sizeof(sMsg));
	memcpy(sMsg.uuid,szUUID,strlen(szUUID));
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


static int GetGPIOData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetGPIOReq sMsg;

	char szNo[8] = {0};

	GetCgiParam((char*)szNo,szCgi,sizeof(szNo),"no=","&");

	sMsg.gpio_no = atoi(szNo);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetGPIOData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlSetGPIOReq sMsg;

	char szNo[8] = {0};
	char szValue[8] = {0};

	GetCgiParam((char*)szNo,szCgi,sizeof(szNo),"no=","&");
	GetCgiParam((char*)szValue,szCgi,sizeof(szValue),"value=","&");

	sMsg.gpio_no = atoi(szNo);
	sMsg.value = atoi(szValue) > 0 ? 1 : 0;
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetWirelessData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetWifiReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetWirelessData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlSetWifiReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	// setwifilist.cgi?ssid=%s&password=%s&mode=%d&enctype=%d;

	char szMode[8] = {0};
	char szType[8] = {0};

	GetCgiParam((char*)sMsg.ssid,szCgi,sizeof(sMsg.ssid),"ssid=","&");
	GetCgiParam((char*)sMsg.password,szCgi,sizeof(sMsg.password),"password=","&");
	GetCgiParam(szMode,szCgi,sizeof(szMode),"mode=","&");
	GetCgiParam(szType,szCgi,sizeof(szType),"type=","&");

	sMsg.enctype = atoi(szType);
	sMsg.mode = atoi(szMode);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


static int SetPushData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlSetPushReq sMsg;

	char szType[8] = {0};
	char szApnsTestEnable[8] = {0};

	memset(&sMsg,0,sizeof(sMsg));
	
	GetCgiParam(sMsg.AppKey,szCgi,sizeof(sMsg.AppKey),"AppKey=","&");
	GetCgiParam(sMsg.Master,szCgi,sizeof(sMsg.Master),"Master=","&");
	GetCgiParam(sMsg.FCMKey,szCgi,sizeof(sMsg.FCMKey),"FCMKey=","&");
	GetCgiParam(sMsg.Groups,szCgi,sizeof(sMsg.Groups),"Alias=" ,"&");
	
	GetCgiParam(szApnsTestEnable,szCgi,sizeof(szApnsTestEnable),"ApnsTestEnabale=","&");
	GetCgiParam(szType,szCgi,sizeof(szType),"Type=","&");
	
	sMsg.Type = atoi(szType);
	sMsg.ApnsTestEnable = atoi(szApnsTestEnable);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int DelPushData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlDelPushReq sMsg;

	char szType[8] = {0};

	memset(&sMsg,0,sizeof(sMsg));
	
	GetCgiParam(sMsg.AppKey,szCgi,sizeof(sMsg.AppKey),"AppKey=","&");
	GetCgiParam(sMsg.Master,szCgi,sizeof(sMsg.Master),"Master=","&");
	GetCgiParam(sMsg.Groups,szCgi,sizeof(sMsg.Groups),"Alias=" ,"&");
	
	GetCgiParam(szType,szCgi,sizeof(szType),"Type=","&");
	sMsg.Type = atoi(szType);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetPTZ(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szControl[8] = {0};
	char szSpeed[8] = {0};

	GetCgiParam(szControl,szCgi,sizeof(szControl),"control=","&");
	GetCgiParam(szSpeed,szCgi,sizeof(szSpeed),"speed=","&");
	
	SMsgAVIoctrlPtzCmd sMsg;
	memset(&sMsg,0,sizeof(sMsg));
	sMsg.control = atoi(szControl);
	sMsg.speed = atoi(szSpeed);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetStreamCtrl(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szChannel[8] = {0};
	char szQuality[8] = {0};

	GetCgiParam(szChannel,szCgi,sizeof(szChannel),"channel=","&");
	GetCgiParam(szQuality,szCgi,sizeof(szQuality),"quality=","&");
	
	SMsgAVIoctrlSetStreamCtrlReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));
	sMsg.channel = atoi(szChannel) > 2 ? 0 : atoi(szChannel);
	sMsg.quality = atoi(szQuality);

//	Log3("tutk cmd set stream channel:[%d] quality:[%d].\n",sMsg.channel,sMsg.quality);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetStreamCtrl(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetStreamCtrlReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetRecordSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szChannel[8] = {0};
	char szType[8] = {0};
	char szStartHour[8] = {0};
	char szStartMins[8] = {0};
	char szCloseHour[8] = {0};
	char szCloseMins[8] = {0};
	char szVideoLens[8] = {0};

	// AVIOCTRL Record Type
	/*
	typedef enum
	{
		AVIOTC_RECORDTYPE_OFF				= 0x00,
		AVIOTC_RECORDTYPE_FULLTIME			= 0x01,
		AVIOTC_RECORDTYPE_ALARM				= 0x02,
		AVIOTC_RECORDTYPE_MANUAL			= 0x03,
		AVIOTC_RECORDTYPE_SCHEDULE			= 0x04
	}ENUM_RECORD_TYPE;
	*/

	GetCgiParam(szChannel,szCgi,sizeof(szChannel),"control=","&");
	GetCgiParam(szType,szCgi,sizeof(szType),"type=","&");
	GetCgiParam(szStartHour,szCgi,sizeof(szStartHour),"startHour=","&");
	GetCgiParam(szStartMins,szCgi,sizeof(szStartMins),"startMins=","&");
	GetCgiParam(szCloseHour,szCgi,sizeof(szCloseHour),"closeHour=","&");
	GetCgiParam(szCloseMins,szCgi,sizeof(szCloseMins),"closeMins=","&");
	GetCgiParam(szVideoLens,szCgi,sizeof(szVideoLens),"videoLens=","&");

	SMsgAVIoctrlSetRecordReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	sMsg.channel = atoi(szChannel);
	sMsg.recordType = atoi(szType);
	sMsg.startHour = atoi(szStartHour);
	sMsg.closeHour = atoi(szCloseHour);
	sMsg.startMins = atoi(szStartMins);
	sMsg.closeMins = atoi(szCloseMins);
	sMsg.videoLens = atoi(szVideoLens);

	Log3("SetRecordSchedule videoLens:%d",sMsg.videoLens);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetRecordSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetRecordReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetLock(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szDoorNumb[8]  = {0};
	char szOpenTime[8]  = {0};
	char szOpenPass[32] = {0};

	SMsgAVIoctrlDoorOpenReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	GetCgiParam(szDoorNumb,szCgi,sizeof(szDoorNumb),"doornumb=","&");
	GetCgiParam(szOpenTime,szCgi,sizeof(szOpenTime),"opentime=","&");
	GetCgiParam(szOpenTime,szCgi,sizeof(szOpenTime),"doorpass=","&");

	sMsg.DoorNumb = atoi(szDoorNumb);
	sMsg.OpenTime = atoi(szOpenTime);
	memcpy(sMsg.DoorPass,szOpenPass,strlen(szOpenPass));

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetLockPass(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szOrigPass[32] = {0};
	char szDoorPass[32] = {0};

	SMsgAVIoctrlDoorPassReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	GetCgiParam(szOrigPass,szCgi,sizeof(szOrigPass),"origpass=","&");
	GetCgiParam(szDoorPass,szCgi,sizeof(szDoorPass),"doorpass=","&");

	memcpy(sMsg.OrigPass,szOrigPass,strlen(szOrigPass));
	memcpy(sMsg.DoorPass,szDoorPass,strlen(szDoorPass));

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetVideo(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szFlipEnable[8] = {0};
	char szMirrEnable[8] = {0};

	GetCgiParam(szFlipEnable,szCgi,sizeof(szFlipEnable),"flip=","&");
	GetCgiParam(szMirrEnable,szCgi,sizeof(szMirrEnable),"mirror=","&");

	int flipEnable = atoi(szFlipEnable) ? 1 : 0;
	int mirrEnable = atoi(szMirrEnable) ? 1 : 0;

	SMsgAVIoctrlSetVideoModeReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));
	
	if(flipEnable){
		sMsg.mode |= AVIOCTRL_VIDEOMODE_FLIP;
	}
	
	if(mirrEnable){
		sMsg.mode |= AVIOCTRL_VIDEOMODE_MIRROR;
	}
		
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetVideo(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetVideoModeReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetSystem(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char szPower[8] = {0};
	char szLanguage[8] = {0};
	char szTimezone[32] = {0};
	char szEnableAutomicUpdate[8] = {0};
	char szEnablePreviewUnlock[8] = {0};
	char szEnableRingingButton[8] = {0};

	GetCgiParam(szPower,szCgi,sizeof(szPower),"power=","&");
	GetCgiParam(szLanguage,szCgi,sizeof(szLanguage),"language=","&");
	GetCgiParam(szTimezone,szCgi,sizeof(szTimezone),"timezone=","&");
	GetCgiParam(szEnableAutomicUpdate,szCgi,sizeof(szEnableAutomicUpdate),"enableAutomicUpdate=","&");
	GetCgiParam(szEnablePreviewUnlock,szCgi,sizeof(szEnablePreviewUnlock),"enablePreviewUnlock=","&");
	GetCgiParam(szEnableRingingButton,szCgi,sizeof(szEnableRingingButton),"enableRingingButton=","&");

	SMsgAVIoctrlSetSystemReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	sMsg.power_ctrl = atoi(szPower);
	sMsg.lang = atoi(szLanguage);
	sMsg.enableAutomicUpdate = atoi(szEnableAutomicUpdate);
	sMsg.enablePreviewUnlock = atoi(szEnablePreviewUnlock);
	sMsg.enableRingingButton = atoi(szEnableRingingButton);
	sMsg.timezone = atoi(szTimezone);

	GetCgiParam(sMsg.datetime,szCgi,sizeof(sMsg.datetime),"datetime=","&");
		
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetSystem(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetSystemReq sMsg;

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetMotionSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	char sMotionEnable[8] = {0};
	char sMotionSensitivity[8] = {0};
	char sMotionNotifyDelay[8] = {0};

	char sMotionStartHour[8] = {0};
	char sMotionCloseHour[8] = {0};
	char sMotionStartMins[8] = {0};
	char sMotionCloseMins[8] = {0};

	GetCgiParam(sMotionEnable,Cgi,sizeof(sMotionEnable),"enable=","&");
	GetCgiParam(sMotionSensitivity,Cgi,sizeof(sMotionSensitivity),"level=","&");
	GetCgiParam(sMotionNotifyDelay,Cgi,sizeof(sMotionNotifyDelay),"delay=","&");
	GetCgiParam(sMotionStartHour,Cgi,sizeof(sMotionStartHour),"startHour=","&");
	GetCgiParam(sMotionCloseHour,Cgi,sizeof(sMotionCloseHour),"closeHour=","&");
	GetCgiParam(sMotionStartMins,Cgi,sizeof(sMotionStartMins),"startMins=","&");
	GetCgiParam(sMotionCloseMins,Cgi,sizeof(sMotionCloseMins),"closeMins=","&");

	SMsgAVIoctrlSetMDPReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	sMsg.MotionAlarmFrequency = atoi(sMotionNotifyDelay) < 0 ? 0 : atoi(sMotionNotifyDelay);
	sMsg.MotionAlarmType = 0;
	sMsg.MotionEnable = atoi(sMotionEnable);
	sMsg.MotionLevel = atoi(sMotionSensitivity) * 20;
	sMsg.MotionStartHour = atoi(sMotionStartHour);
	sMsg.MotionStartMins = atoi(sMotionStartMins);
	sMsg.MotionCloseHour = atoi(sMotionCloseHour);
	sMsg.MotionCloseMins = atoi(sMotionCloseMins);

	avSendIOCtrl(avIdx,IOTYPE_USER_IPCAM_SETMOTIONDETECT_REQ,(const char *)&sMsg,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetMotionScheduleEx(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	char sMotionEnable[8] = {0};
	char sMotionSensitivity[8] = {0};
	char sMotionNotifyDelay[8] = {0};
	char sMotionAudioOutput[8] = {0};
	char sMotionNotify[8] = {0};
	char sMotionRecord[8] = {0};
	char sEnablePir[8] = {0};
	char sEnableRemoveAlarm[8] = {0};

	char sMotionStartHour[8] = {0};
	char sMotionCloseHour[8] = {0};
	char sMotionStartMins[8] = {0};
	char sMotionCloseMins[8] = {0};

	GetCgiParam(sMotionEnable,Cgi,sizeof(sMotionEnable),"enable=","&");
	GetCgiParam(sMotionSensitivity,Cgi,sizeof(sMotionSensitivity),"level=","&");
	GetCgiParam(sMotionNotifyDelay,Cgi,sizeof(sMotionNotifyDelay),"delay=","&");
	GetCgiParam(sMotionNotify,Cgi,sizeof(sMotionNotify),"notify=","&");
	GetCgiParam(sMotionRecord,Cgi,sizeof(sMotionRecord),"record=","&");
	GetCgiParam(sMotionAudioOutput,Cgi,sizeof(sMotionAudioOutput),"audio=","&");
	GetCgiParam(sEnablePir,Cgi,sizeof(sEnablePir),"enablePir=","&");
	GetCgiParam(sEnableRemoveAlarm,Cgi,sizeof(sEnableRemoveAlarm),"removeAlarm=","&");
	GetCgiParam(sMotionStartHour,Cgi,sizeof(sMotionStartHour),"startHour=","&");
	GetCgiParam(sMotionCloseHour,Cgi,sizeof(sMotionCloseHour),"closeHour=","&");
	GetCgiParam(sMotionStartMins,Cgi,sizeof(sMotionStartMins),"startMins=","&");
	GetCgiParam(sMotionCloseMins,Cgi,sizeof(sMotionCloseMins),"closeMins=","&");

	SMsgAVIoctrlMDAlarmReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	sMsg.MotionAlarmFrequency = atoi(sMotionNotifyDelay) < 0 ? 0 : atoi(sMotionNotifyDelay);
	sMsg.MotionEnable = atoi(sMotionEnable);
	sMsg.MotionLevel = atoi(sMotionSensitivity) * 20;
	sMsg.MotionNotify = atoi(sMotionNotify);
	sMsg.MotionRecord = atoi(sMotionRecord);
	sMsg.EnablePir = atoi(sEnablePir);
	sMsg.EnableRemoveAlarm = atoi(sEnableRemoveAlarm);
	sMsg.MotionAudioOutput = atoi(sMotionAudioOutput);
	sMsg.MotionStartHour = atoi(sMotionStartHour);
	sMsg.MotionStartMins = atoi(sMotionStartMins);
	sMsg.MotionCloseHour = atoi(sMotionCloseHour);
	sMsg.MotionCloseMins = atoi(sMotionCloseMins);

	Log3("SetMotionScheduleEx PIR:%d",sMsg.EnablePir);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetMotionSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlGetMDPReq sMsg;

	avSendIOCtrl(avIdx,IOTYPE_USER_IPCAM_GETMOTIONDETECT_REQ,(const char *)&sMsg,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetMotionScheduleEx(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlMDAlarmReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


static int GetTimeZone(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlTimeZone sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetTimeZone(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlTimeZone sMsg;

	char sTimeZone[8] = {0};

	GetCgiParam(sTimeZone,Cgi,sizeof(sTimeZone),"timezone=","&");
	
	sMsg.nTimeZone = atoi(sTimeZone);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int XetSDCard(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlFormatExtStorageReq sMsg;
	sMsg.storage = 0;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetAPList(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	SMsgAVIoctrlListWifiApReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetSoundMode(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSoundCtrlReq sMsg;

	char szMode[8] = {0};
	GetCgiParam(szMode,Cgi,sizeof(szMode),"mode=","&");

	memset(&sMsg,0,sizeof(sMsg));
	sMsg.mode = atoi(szMode);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetPassword(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSetPasswdReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	GetCgiParam(sMsg.user,Cgi,sizeof(sMsg.user),"user=","&");
	GetCgiParam(sMsg.oldpasswd,Cgi,sizeof(sMsg.oldpasswd),"oldpass=","&");
	GetCgiParam(sMsg.newpasswd,Cgi,sizeof(sMsg.newpasswd),"newpass=","&");
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetOSD(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSetOSDReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szChannel[8] = {0};

	GetCgiParam(szChannel,Cgi,sizeof(szChannel),"channel=","&");
	sMsg.channel = atoi(szChannel);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetOSD(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlGetOSDReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szChannel[8] = {0};

	/*
	char szChannelNameEnable[8] = {0};
	char szChannelNameX[8] = {0};
	char szChannelNameY[8] = {0};
	char szDateTimeEnabel[8] = {0};
	char szDateFormat[8] = {0};
	char szDateSprtr[8] = {0};
	char szTimeFormat[8] = {0};
	char szDisplayWeek[8] = {0};
	char szDateTimeX[8] = {0};
	char szDateTimeY[8] = {0};
	char szChannelIDEnabel[8] = {0};
	char szChannelIDX[8] = {0};
	char szChannelIDY[8] = {0};
	*/

	GetCgiParam(szChannel,Cgi,sizeof(szChannel),"channel=","&");
	GetCgiParam(sMsg.channel_name_text,Cgi,sizeof(sMsg.channel_name_text),"channel_name_text=","&");
	
	sMsg.channel = atoi(szChannel);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int ParingRFDevExit(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlParingRFExitReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int ParingRFDev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlParingRFReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szName[512] = {0};
	char szType[  8] = {0};

	GetCgiParam(szName,Cgi,sizeof(szName),"name=","&");
	GetCgiParam(szType,Cgi,sizeof(szType),"type=","&");

	memcpy(sMsg.name,szName,strlen(szName) > sizeof(sMsg.name) ? sizeof(sMsg.name) - 1 : strlen(szName));
	sMsg.type = atoi(szType);

	Log3("433 ADD NEW DEVICE WITH NICK NAME:[%s].\n",sMsg.name);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// get433.cgi?
static int SelectRFDev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSelectRFReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// cfg433.cgi?
static int ConfigRFDev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlConfigRFReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szName[512] = {0};
	char szType[  8] = {0};
	char szUUID[  8] = {0};

	GetCgiParam(szUUID,Cgi,sizeof(szUUID),"uuid=","&");
	GetCgiParam(szName,Cgi,sizeof(szName),"name=","&");
	GetCgiParam(szType,Cgi,sizeof(szType),"type=","&");

	memcpy(sMsg.name,szName,strlen(szName) > sizeof(sMsg.name) ? sizeof(sMsg.name) - 1 : strlen(szName));
	sMsg.type = atoi(szType);
	sMsg.id = atoi(szUUID);

	Log3("433 CFG OLD DEVICE WITH NICK NAME:[%s].\n",sMsg.name);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// del433.cgi?id=%d
static int RemoveRFDev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	int				CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlRemoveRFReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szID[32] = {0};

	GetCgiParam(szID,Cgi,sizeof(szID),"id=","&");

	sMsg.id = atoi(szID);

	Log3("user del 433 device:[%d].\n",sMsg.id);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetUpdateUrl(
     int			avIdx,
     int			avMsgType,
     const char *	szCgi,
     int			CgiLens,
     void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlUpdateReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));
    
    char sUpdateType[8] = {0};
    
    GetCgiParam(sUpdateType,Cgi,sizeof(sUpdateType),"type=","&");
    GetCgiParam(sMsg.url,Cgi,sizeof(sMsg.url),"url=","&");
    GetCgiParam(sMsg.md5,Cgi,sizeof(sMsg.md5),"md5=","&");
    
    sMsg.updateType = atoi(sUpdateType);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetUpdateProgress(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlUpdateProgReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetCapacity(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlGetCapacityReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetPlayRecordControl(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlPlayRecord sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));

	char sChannel[8] = {0};
	char sCommand[8] = {0};
	char sParam[8] = {0};

	char sEventTime[128] = {0};

/*
	typedef enum{
		AVIOCTRL_RECORD_PLAY_PAUSE			= 0x00,
		AVIOCTRL_RECORD_PLAY_STOP			= 0x01,
		AVIOCTRL_RECORD_PLAY_STEPFORWARD	= 0x02, //now, APP no use
		AVIOCTRL_RECORD_PLAY_STEPBACKWARD	= 0x03, //now, APP no use
		AVIOCTRL_RECORD_PLAY_FORWARD		= 0x04, //now, APP no use
		AVIOCTRL_RECORD_PLAY_BACKWARD		= 0x05, //now, APP no use
		AVIOCTRL_RECORD_PLAY_SEEKTIME		= 0x06, //now, APP no use
		AVIOCTRL_RECORD_PLAY_END			= 0x07,
		AVIOCTRL_RECORD_PLAY_START			= 0x10,
	}ENUM_PLAYCONTROL;
*/
    
	GetCgiParam(sChannel,Cgi,sizeof(sChannel),"channel=","&");
    GetCgiParam(sCommand,Cgi,sizeof(sCommand),"command=","&");
    GetCgiParam(sParam,Cgi,sizeof(sParam),"param=","&");
	GetCgiParam(sEventTime,Cgi,sizeof(sEventTime),"eventtime=","&");
	
	sMsg.channel = atoi(sChannel);
	sMsg.command = atoi(sCommand);
	sMsg.Param = atoi(sParam);

	sscanf(sEventTime,"%d-%d-%d %d:%d:%d",
		(int *)&sMsg.stTimeDay.year,
		(int *)&sMsg.stTimeDay.month,
		(int *)&sMsg.stTimeDay.day,
		(int *)&sMsg.stTimeDay.hour,
		(int *)&sMsg.stTimeDay.minute,
		(int *)&sMsg.stTimeDay.second
		);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetEventList(
	int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
	char * Cgi = (char*)szCgi;
	SMsgAVIoctrlListEventReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	char sStartTime[128] = {0};
	char sCloseTime[128] = {0};
	
	char sEventType[8] = {0};
	char sEventStatus[8] = {0};

	/*
	typedef enum 
	{
		AVIOCTRL_EVENT_ALL					= 0x00,	// all event type(general APP-->IPCamera)
		AVIOCTRL_EVENT_MOTIONDECT			= 0x01,	// motion detect start//==s==
		AVIOCTRL_EVENT_VIDEOLOST			= 0x02,	// video lost alarm
		AVIOCTRL_EVENT_IOALARM				= 0x03, // io alarmin start //---s--

		AVIOCTRL_EVENT_MOTIONPASS			= 0x04, // motion detect end  //==e==
		AVIOCTRL_EVENT_VIDEORESUME			= 0x05,	// video resume
		AVIOCTRL_EVENT_IOALARMPASS			= 0x06, // IO alarmin end   //---e--

		AVIOCTRL_EVENT_EXPT_REBOOT			= 0x10, // system exception reboot
		AVIOCTRL_EVENT_SDFAULT				= 0x11, // sd record exception
	}ENUM_EVENTTYPE;
	*/

	GetCgiParam(sEventType,Cgi,sizeof(sEventType),"eventType=","&");
	GetCgiParam(sEventStatus,Cgi,sizeof(sEventStatus),"eventStatus=","&");

	GetCgiParam(sStartTime,Cgi,sizeof(sStartTime),"starttime=","&");
	GetCgiParam(sCloseTime,Cgi,sizeof(sCloseTime),"closetime=","&");

	Log3("event get start time:%s.\n",sStartTime);
	Log3("event get close time:%s.\n",sCloseTime);

	sscanf(sStartTime,"%d-%d-%d %d:%d:%d",
		&sMsg.stStartTime.year,
		&sMsg.stStartTime.month,
		&sMsg.stStartTime.day,
		&sMsg.stStartTime.hour,
		&sMsg.stStartTime.minute,
		&sMsg.stStartTime.second
		);

	sscanf(sCloseTime,"%d-%d-%d %d:%d:%d",
		&sMsg.stEndTime.year,
		&sMsg.stEndTime.month,
		&sMsg.stEndTime.day,
		&sMsg.stEndTime.hour,
		&sMsg.stEndTime.minute,
		&sMsg.stEndTime.second
		);
	
	sMsg.event = atoi(sEventType);
	sMsg.status = atoi(sEventStatus);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
	
}

static int GetEventListByMonth(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlEventByMonthReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));

	char sYearMonth[8] = {0};

	GetCgiParam(sYearMonth,Cgi,sizeof(sYearMonth),"time=","&");

	sscanf(sYearMonth,"%d-%d",
		&sMsg.year,
		&sMsg.month
		);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


static int SetPresetPostion(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlSetPresetReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));

	char sIdx[8] = {0};
	GetCgiParam(sIdx,Cgi,sizeof(sIdx),"index=","&");

	sMsg.nPresetIdx = atoi(sIdx);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetPresetPostion(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlGetPresetReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));

	char sIdx[8] = {0};
	GetCgiParam(sIdx,Cgi,sizeof(sIdx),"index=","&");

	sMsg.nPresetIdx = atoi(sIdx);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetCameraView(
    int             avIdx,
    int             avMsgType,
    const char *	szCgi,
    int				CgiLens,
    void *			lpParams
){
    char * Cgi = (char*)szCgi;
    
    SMsgAVIoctrlGetCameraViewReq sMsg;
    
    memset(&sMsg,0,sizeof(sMsg));

	char sIdx[8] = {0};
	GetCgiParam(sIdx,Cgi,sizeof(sIdx),"index=","&");

	sMsg.index = atoi(sIdx);
    
    return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int XMCallResp(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlCallResp sMsg;
	
	memset(&sMsg,0,sizeof(sMsg));

	char sIdx[8] = {0};
	char sAck[8] = {0};
	GetCgiParam(sIdx,Cgi,sizeof(sIdx),"index=","&");
	GetCgiParam(sAck,Cgi,sizeof(sAck),"ack=","&");

	sMsg.index = atoi(sIdx);
	sMsg.nAnswered = atoi(sAck);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetBatteryStatus(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlCallResp sMsg;
	
	memset(&sMsg,0,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetAudioVolume(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlGetAudioVolumeReq sMsg;
	
	memset(&sMsg,0,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetAudioVolume(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlSetAudioVolumeReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char sAudioVolume[8] = {0};
	GetCgiParam(sAudioVolume,Cgi,sizeof(sAudioVolume),"audioVolume=","&");

	sMsg.level = atoi(sAudioVolume);

	Log3("SetAudioVolume:%d",sMsg.level);
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetAudioGain(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlGetAudioGainReq sMsg;
	
	memset(&sMsg,0,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetAudioGain(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlSetAudioGainReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char sAudioGain[8] = {0};
	GetCgiParam(sAudioGain,Cgi,sizeof(sAudioGain),"audioGain=","&");

	sMsg.level = atoi(sAudioGain);

	Log3("SetAudioGain:%d",sMsg.level);
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


static int GetWakeUpOptions(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlGetWakeUpStateReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetWakeUpOptions(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlSetWakeUpStateReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char sWakeUpEnable[8] = {0};
	GetCgiParam(sWakeUpEnable,Cgi,sizeof(sWakeUpEnable),"enable=","&");

	sMsg.enable = atoi(sWakeUpEnable);

	Log3("sWakeUpEnable:%d",sMsg.enable);
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int GetEnv(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlGetEnvironmentReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char sChannel[8] = {0};
	GetCgiParam(sChannel,Cgi,sizeof(sChannel),"channel=","&");

	sMsg.channel = atoi(sChannel);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

static int SetEnv(
	int 			avIdx,
	int 			avMsgType,
	const char *	szCgi,
	int 			CgiLens,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;
	
	SMsgAVIoctrlSetEnvironmentReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char sChannel[8] = {0};
	char sMode[8] = {0};

	GetCgiParam(sChannel,Cgi,sizeof(sChannel),"channel=","&");
	GetCgiParam(sMode,Cgi,sizeof(sMode),"mode=","&");

	sMsg.channel = atoi(sChannel);
	sMsg.mode = atoi(sMode);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


// fucntion list for each command

static APP_CMD_CALL hACC[] = {
	{IOTYPE_USER_IPCAM_SET_UUID,SetUUIDData},
	{IOTYPE_USER_IPCAM_GET_GPIO,GetGPIOData},
	{IOTYPE_USER_IPCAM_SET_GPIO,SetGPIOData},
	{IOTYPE_USER_IPCAM_SETWIFI_REQ,SetWirelessData},
	{IOTYPE_USER_IPCAM_GETWIFI_REQ,GetWirelessData},
	{IOTYPE_USER_IPCAM_SETWIFI_REQ,SetWirelessData},
	{IOTYPE_USER_IPCAM_SET_PUSH_REQ,SetPushData},
	{IOTYPE_USER_IPCAM_DEL_PUSH_REQ,DelPushData},
	{IOTYPE_USER_IPCAM_PTZ_COMMAND,SetPTZ},
	{IOTYPE_USER_IPCAM_SETSTREAMCTRL_REQ,SetStreamCtrl},
	{IOTYPE_USER_IPCAM_GETSTREAMCTRL_REQ,GetStreamCtrl},
	{IOTYPE_USER_IPCAM_SETRECORD_REQ,SetRecordSchedule},
	{IOTYPE_USER_IPCAM_GETRECORD_REQ,GetRecordSchedule},
	{IOTYPE_USER_IPCAM_SET_MDP_REQ,SetMotionSchedule},
	{IOTYPE_USER_IPCAM_GET_MDP_REQ,GetMotionSchedule},
	{IOTYPE_USER_IPCAM_SET_MD_ALAM_REQ,SetMotionScheduleEx},
	{IOTYPE_USER_IPCAM_GET_MD_ALAM_REQ,GetMotionScheduleEx},
	{IOTYPE_USER_IPCAM_DOOROPEN_REQ,SetLock},
	{IOTYPE_USER_IPCAM_DOORPASS_REQ,SetLockPass},
	{IOTYPE_USER_IPCAM_SET_VIDEOMODE_REQ,SetVideo},
	{IOTYPE_USER_IPCAM_GET_VIDEOMODE_REQ,GetVideo},
	{IOTYPE_USER_IPCAM_SET_SYSTEM_REQ,SetSystem},
	{IOTYPE_USER_IPCAM_GET_SYSTEM_REQ,GetSystem},
	{IOTYPE_USER_IPCAM_GET_TIMEZONE_REQ,GetTimeZone},
	{IOTYPE_USER_IPCAM_SET_TIMEZONE_REQ,SetTimeZone},
	{IOTYPE_USER_IPCAM_FORMATEXTSTORAGE_REQ,XetSDCard},
	{IOTYPE_USER_IPCAM_GET_SDCARD_REQ,XetSDCard},
	{IOTYPE_USER_IPCAM_LISTWIFIAP_REQ,GetAPList},
	{IOTYPE_USER_IPCAM_SOUND_CTRL,SetSoundMode},
	{IOTYPE_USER_IPCAM_SETPASSWORD_REQ,SetPassword},
	{IOTYPE_USER_IPCAM_GET_OSD_REQ,GetOSD},
	{IOTYPE_USER_IPCAM_SET_OSD_REQ,SetOSD},
	{IOTYPE_USER_IPCAM_PARING_RF_REQ,ParingRFDev},	
	{IOTYPE_USER_IPCAM_SELECT_RF_REQ,SelectRFDev},	
	{IOTYPE_USER_IPCAM_CONFIG_RF_REQ,ConfigRFDev},
	{IOTYPE_USER_IPCAM_REMOVE_RF_REQ,RemoveRFDev},
	{IOTYPE_USER_IPCAM_PARING_RF_EXIT_REQ,ParingRFDevExit},
    {IOTYPE_USER_IPCAM_UPDATE_REQ,SetUpdateUrl},
    {IOTYPE_USER_IPCAM_UPDATE_PROG_REQ,GetUpdateProgress},
    {IOTYPE_USER_IPCAM_GET_CAPACITY_REQ,GetCapacity},
	{IOTYPE_USER_IPCAM_RECORD_PLAYCONTROL,SetPlayRecordControl},
	{IOTYPE_USER_IPCAM_LISTEVENT_REQ,GetEventList},
	{IOTYPE_USER_IPCAM_LISTEVENT_BY_MONTH_REQ,GetEventListByMonth},
	{IOTYPE_USER_IPCAM_SETPRESET_REQ,SetPresetPostion},
	{IOTYPE_USER_IPCAM_GETPRESET_REQ,GetPresetPostion},
	{IOTYPE_USER_IPCAM_GET_CAMERA_VIEW_REQ,GetCameraView},
	{IOTYPE_XM_CALL_RESP,XMCallResp},
	{IOTYPE_USER_IPCAM_GET_BATTERY_REQ,GetBatteryStatus},
	{IOTYPE_USER_IPCAM_GET_AUDIO_VOLUME_REQ,GetAudioVolume},
	{IOTYPE_USER_IPCAM_SET_AUDIO_VOLUME_REQ,SetAudioVolume},
	{IOTYPE_USER_IPCAM_GET_AUDIO_GAIN_REQ,GetAudioGain},
	{IOTYPE_USER_IPCAM_SET_AUDIO_GAIN_REQ,SetAudioGain},
	{IOTYPE_USER_IPCAM_GET_WAKEUP_FUN_REQ,GetWakeUpOptions},
	{IOTYPE_USER_IPCAM_SET_WAKEUP_FUN_REQ,SetWakeUpOptions},
	{IOTYPE_USER_IPCAM_GET_ENVIRONMENT_REQ,GetEnv},
	{IOTYPE_USER_IPCAM_SET_ENVIRONMENT_REQ,SetEnv},
	{0,NULL}
};

int SendCmds(
	int 			Idx,
	int				MsgType,
	const char *	Cgi,
	int				CgiLens,
	void *			Params
){
//	Log3("CCCCCCCCCCCCall SendCmds:[%08X].\n",MsgType);

	int i = 0;
	for(i = 0;hACC[i].CmdCall != NULL;i++){
		if(MsgType == hACC[i].CmdType){

//			Log3("app cmd send IOCTRL cmd:[%03X].",hACC[i].CmdType);
			
			return hACC[i].CmdCall(
				Idx,
				hACC[i].CmdType,
				Cgi,
				CgiLens,
				Params
				);
		}
	}

    return -1;
}
