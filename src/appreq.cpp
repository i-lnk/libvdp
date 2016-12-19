#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utility.h"
#include "appreq.h"

#include "IOTCAPIs.h"
#include "AVAPIs.h"
#include "AVFRAMEINFO.h"
#include "AVIOCTRLDEFs.h"

//
//	string safe copy between string
//
char * StringSafeCopyEx(
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

/*
	TRACE("COPY DATE FROM:[%c][%d].\n",
		lpBegin[0],CopyLens);
*/	
	memcpy(To,lpBegin,CopyLens);

	return To;
}

int SetUUIDData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlSetUUIDReq sMsg;

	char szUUID[64] = {0};

	// public static int IOTYPE_USER_IPCAM_SET_UUID	= 0x9999;	//  ÷ª˙∂À∑¢ÀÕ…Ë÷√…Ë±∏UUIDµƒ«Î«Û
	
	// String msg = "setuuid.cgi?UUID=" + UUID + "&";
	// Avapi.SendCtrlCommand(ConnectService.device_id, Constant.IOTYPE_USER_IPCAM_SET_UUID, msg, msg.length());

	StringSafeCopyEx((char*)szUUID,szCgi,sizeof(szUUID),"UUID=","&");

	memset(&sMsg,0,sizeof(sMsg));
	memcpy(sMsg.uuid,szUUID,strlen(szUUID));
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


int GetGPIOData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetGPIOReq sMsg;

	char szNo[8] = {0};

	StringSafeCopyEx((char*)szNo,szCgi,sizeof(szNo),"no=","&");

	sMsg.gpio_no = atoi(szNo);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetGPIOData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlSetGPIOReq sMsg;

	char szNo[8] = {0};
	char szValue[8] = {0};

	StringSafeCopyEx((char*)szNo,szCgi,sizeof(szNo),"no=","&");
	StringSafeCopyEx((char*)szValue,szCgi,sizeof(szValue),"value=","&");

	sMsg.gpio_no = atoi(szNo);
	sMsg.value = atoi(szValue) > 0 ? 1 : 0;
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetWirelessData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetWifiReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetWirelessData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlSetWifiReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	// setwifilist.cgi?ssid=%s&password=%s&mode=%d&enctype=%d;

	char szMode[8] = {0};
	char szType[8] = {0};

	StringSafeCopyEx((char*)sMsg.ssid,szCgi,sizeof(sMsg.ssid),"ssid=","&");
	StringSafeCopyEx((char*)sMsg.password,szCgi,sizeof(sMsg.password),"password=","&");
	StringSafeCopyEx(szMode,szCgi,sizeof(szMode),"mode=","&");
	StringSafeCopyEx(szType,szCgi,sizeof(szType),"type=","&");

	sMsg.enctype = atoi(szType);
	sMsg.mode = atoi(szMode);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}


int SetPushData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlSetPushReq sMsg;

	char szType[8] = {0};

	memset(&sMsg,0,sizeof(sMsg));
	
	StringSafeCopyEx(sMsg.AppKey,szCgi,sizeof(sMsg.AppKey),"AppKey=","&");
	StringSafeCopyEx(sMsg.Master,szCgi,sizeof(sMsg.Master),"Master=","&");
	StringSafeCopyEx(sMsg.Alias, szCgi,sizeof(sMsg.Alias ),"Alias=" ,"&");
	
	StringSafeCopyEx(szType,szCgi,sizeof(szType),"Type=","&");
	sMsg.Type = atoi(szType);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int DelPushData(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlDelPushReq sMsg;

	char szType[8] = {0};

	memset(&sMsg,0,sizeof(sMsg));
	
	StringSafeCopyEx(sMsg.AppKey,szCgi,sizeof(sMsg.AppKey),"AppKey=","&");
	StringSafeCopyEx(sMsg.Master,szCgi,sizeof(sMsg.Master),"Master=","&");
	StringSafeCopyEx(sMsg.Alias, szCgi,sizeof(sMsg.Alias ),"Alias=" ,"&");
	
	StringSafeCopyEx(szType,szCgi,sizeof(szType),"Type=","&");
	sMsg.Type = atoi(szType);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetPTZ(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szControl[8] = {0};
	char szSpeed[8] = {0};

	StringSafeCopyEx(szControl,szCgi,sizeof(szControl),"control=","&");
	StringSafeCopyEx(szSpeed,szCgi,sizeof(szSpeed),"speed=","&");
	
	SMsgAVIoctrlPtzCmd sMsg;
	memset(&sMsg,0,sizeof(sMsg));
	sMsg.control = atoi(szControl);
	sMsg.speed = atoi(szSpeed);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetStreamCtrl(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szChannel[8] = {0};
	char szQuality[8] = {0};

	StringSafeCopyEx(szChannel,szCgi,sizeof(szChannel),"channel=","&");
	StringSafeCopyEx(szQuality,szCgi,sizeof(szQuality),"quality=","&");
	
	SMsgAVIoctrlSetStreamCtrlReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));
	sMsg.channel = atoi(szChannel) > 2 ? 0 : atoi(szChannel);
	sMsg.quality = atoi(szQuality);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetStreamCtrl(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetStreamCtrlReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetRecordSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szChannel[8] = {0};
	char szType[8] = {0};
	char szStartHour[8] = {0};
	char szStartMins[8] = {0};
	char szCloseHour[8] = {0};
	char szCloseMins[8] = {0};

	StringSafeCopyEx(szChannel,szCgi,sizeof(szChannel),"control=","&");
	StringSafeCopyEx(szType,szCgi,sizeof(szType),"type=","&");
	StringSafeCopyEx(szStartHour,szCgi,sizeof(szStartHour),"startHour=","&");
	StringSafeCopyEx(szStartMins,szCgi,sizeof(szStartMins),"startMins=","&");
	StringSafeCopyEx(szCloseHour,szCgi,sizeof(szCloseHour),"closeHour=","&");
	StringSafeCopyEx(szCloseMins,szCgi,sizeof(szCloseMins),"closeMins=","&");

	SMsgAVIoctrlSetRecordReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	sMsg.channel = atoi(szChannel);
	sMsg.recordType = atoi(szType);
	sMsg.startHour = atoi(szStartHour);
	sMsg.closeHour = atoi(szCloseHour);
	sMsg.startMins = atoi(szStartMins);
	sMsg.closeMins = atoi(szCloseMins);

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetRecordSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetRecordReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetLock(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szDoorNumb[8]  = {0};
	char szOpenTime[8]  = {0};
	char szOpenPass[32] = {0};

	SMsgAVIoctrlDoorOpenReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	StringSafeCopyEx(szDoorNumb,szCgi,sizeof(szDoorNumb),"doornumb=","&");
	StringSafeCopyEx(szOpenTime,szCgi,sizeof(szOpenTime),"opentime=","&");
	StringSafeCopyEx(szOpenTime,szCgi,sizeof(szOpenTime),"doorpass=","&");

	sMsg.DoorNumb = atoi(szDoorNumb);
	sMsg.OpenTime = atoi(szOpenTime);
	memcpy(sMsg.DoorPass,szOpenPass,strlen(szOpenPass));

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetLockPass(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szOrigPass[32]  = {0};
	char szDoorPass[32]  = {0};

	SMsgAVIoctrlDoorPassReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	StringSafeCopyEx(szOrigPass,szCgi,sizeof(szOrigPass),"origpass=","&");
	StringSafeCopyEx(szDoorPass,szCgi,sizeof(szDoorPass),"doorpass=","&");

	memcpy(sMsg.OrigPass,szOrigPass,strlen(szOrigPass));
	memcpy(sMsg.DoorPass,szDoorPass,strlen(szDoorPass));

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetVideo(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szFlipEnable[8] = {0};
	char szMirrEnable[8] = {0};

	StringSafeCopyEx(szFlipEnable,szCgi,sizeof(szFlipEnable),"flip=","&");
	StringSafeCopyEx(szMirrEnable,szCgi,sizeof(szMirrEnable),"mirror=","&");

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

int GetVideo(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetVideoModeReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetSystem(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char szPower[8] = {0};
	char szLanguage[8] = {0};

	StringSafeCopyEx(szPower,szCgi,sizeof(szPower),"power=","&");
	StringSafeCopyEx(szLanguage,szCgi,sizeof(szLanguage),"language=","&");

	SMsgAVIoctrlSetSystemReq sMsg;

	sMsg.power_ctrl = atoi(szPower);
	sMsg.lang = atoi(szLanguage);

	StringSafeCopyEx(sMsg.datetime,szCgi,sizeof(sMsg.datetime),"datetime=","&");
		
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetSystem(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetSystemReq sMsg;

	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetMotionSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
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

	StringSafeCopyEx(sMotionEnable,Cgi,sizeof(sMotionEnable),"enable=","&");
	StringSafeCopyEx(sMotionSensitivity,Cgi,sizeof(sMotionSensitivity),"level=","&");
	StringSafeCopyEx(sMotionNotifyDelay,Cgi,sizeof(sMotionNotifyDelay),"delay=","&");
	StringSafeCopyEx(sMotionStartHour,Cgi,sizeof(sMotionStartHour),"startHour=","&");
	StringSafeCopyEx(sMotionCloseHour,Cgi,sizeof(sMotionCloseHour),"closeHour=","&");
	StringSafeCopyEx(sMotionStartMins,Cgi,sizeof(sMotionStartMins),"startMins=","&");
	StringSafeCopyEx(sMotionCloseMins,Cgi,sizeof(sMotionCloseMins),"closeMins=","&");

	SMsgAVIoctrlSetMDPReq sMsg;
	memset(&sMsg,0,sizeof(sMsg));

	sMsg.MotionAlarmFrequency = atoi(sMotionNotifyDelay) < 5 ? 5 : atoi(sMotionNotifyDelay);
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

int GetMotionSchedule(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlGetMDPReq sMsg;

	avSendIOCtrl(avIdx,IOTYPE_USER_IPCAM_GETMOTIONDETECT_REQ,(const char *)&sMsg,sizeof(sMsg));
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetTimeZone(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlTimeZone sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetTimeZone(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlTimeZone sMsg;

	char sTimeZone[8] = {0};

	StringSafeCopyEx(sTimeZone,Cgi,sizeof(sTimeZone),"timezone=","&");
	
	sMsg.nTimeZone = atoi(sTimeZone);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int XetSDCard(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlTimeZone sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SerialSend(
	int				avIdx,
	int				avMsgType,
	const char *	szMsg,
	void *			lpParams
){
	SMsgAVIoctrlSerialSendReq sMsg;

	PAPP_BIN_DATA hABD = (PAPP_BIN_DATA)szMsg;
	sMsg.size = hABD->lens;
	sMsg.serial_no = 0;
	memcpy(sMsg.data,hABD->d,sMsg.size);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetAPList(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	SMsgAVIoctrlListWifiApReq sMsg;
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetSoundMode(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSoundCtrlReq sMsg;

	char szMode[8] = {0};
	StringSafeCopyEx(szMode,Cgi,sizeof(szMode),"mode=","&");

	memset(&sMsg,0,sizeof(sMsg));
	sMsg.mode = atoi(szMode);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetPassword(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSetPasswdReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	StringSafeCopyEx(sMsg.user,Cgi,sizeof(sMsg.user),"user=","&");
	StringSafeCopyEx(sMsg.oldpasswd,Cgi,sizeof(sMsg.oldpasswd),"oldpass=","&");
	StringSafeCopyEx(sMsg.newpasswd,Cgi,sizeof(sMsg.newpasswd),"newpass=","&");
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int GetOSD(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSetOSDReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szChannel[8] = {0};

	StringSafeCopyEx(szChannel,Cgi,sizeof(szChannel),"channel=","&");
	sMsg.channel = atoi(szChannel);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int SetOSD(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
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

	StringSafeCopyEx(szChannel,Cgi,sizeof(szChannel),"channel=","&");
	StringSafeCopyEx(sMsg.channel_name_text,Cgi,sizeof(sMsg.channel_name_text),"channel_name_text=","&");
	
	sMsg.channel = atoi(szChannel);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// set433dev.cgi?type=%d&name=%d
int Set433Dev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlSet433Req sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szName[512] = {0};
	char szType[  8] = {0};

	StringSafeCopyEx(szName,Cgi,sizeof(szName),"name=","&");
	StringSafeCopyEx(szType,Cgi,sizeof(szType),"type=","&");

	memcpy(sMsg.name,szName,strlen(szName) > sizeof(sMsg.name) ? sizeof(sMsg.name) - 1 : strlen(szName));
	sMsg.type = atoi(szType);

	Log3("433 ADD NEW DEVICE WITH NICK NAME:[%s].\n",sMsg.name);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// get433.cgi?
int Get433Dev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlGet433Req sMsg;

	memset(&sMsg,0,sizeof(sMsg));
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// cfg433.cgi?
int Cfg433Dev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlCfg433Req sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szName[512] = {0};
	char szType[  8] = {0};

	StringSafeCopyEx(szName,Cgi,sizeof(szName),"name=","&");
	StringSafeCopyEx(szType,Cgi,sizeof(szType),"type=","&");

	memcpy(sMsg.name,szName,strlen(szName) > sizeof(sMsg.name) ? sizeof(sMsg.name) - 1 : strlen(szName));
	sMsg.type = atoi(szType);

	Log3("433 CFG OLD DEVICE WITH NICK NAME:[%s].\n",sMsg.name);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

// del433.cgi?id=%d
int Del433Dev(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlDel433Req sMsg;

	memset(&sMsg,0,sizeof(sMsg));

	char szID[8] = {0};

	StringSafeCopyEx(szID,Cgi,sizeof(szID),"id=","&");

	sMsg.id = atoi(szID);

	Log3("user del 433 device:[%d].\n",sMsg.id);
	
	return avSendIOCtrl(avIdx,avMsgType,(const char *)&sMsg,sizeof(sMsg));
}

int Cfg433DevExit(
	int				avIdx,
	int				avMsgType,
	const char *	szCgi,
	void *			lpParams
){
	char * Cgi = (char*)szCgi;

	SMsgAVIoctrlCfg433ExitReq sMsg;

	memset(&sMsg,0,sizeof(sMsg));

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
	{IOTYPE_USER_IPCAM_SERIAL_SEND_REQ,SerialSend},
	{IOTYPE_USER_IPCAM_LISTWIFIAP_REQ,GetAPList},
	{IOTYPE_USER_IPCAM_SOUND_CTRL,SetSoundMode},
	{IOTYPE_USER_IPCAM_SETPASSWORD_REQ,SetPassword},
	{IOTYPE_USER_IPCAM_GET_OSD_REQ,GetOSD},
	{IOTYPE_USER_IPCAM_SET_OSD_REQ,SetOSD},
	{IOTYPE_USER_IPCAM_SET_433_REQ,Set433Dev},	// …Ë÷√ 433 …Ë±∏
	{IOTYPE_USER_IPCAM_GET_433_REQ,Get433Dev},	// ªÒ»° 433 …Ë±∏¡–±Ì
	{IOTYPE_USER_IPCAM_CFG_433_REQ,Cfg433Dev},	// ø™ º 433 ≈‰∂‘
	{IOTYPE_USER_IPCAM_DEL_433_REQ,Del433Dev},  // …æ≥˝ 433 …Ë±∏
	{IOTYPE_USER_IPCAM_CFG_433_EXIT_REQ,Cfg433DevExit},	// ÕÀ≥ˆ 433 …Ë±∏≈‰∂‘
	{0,NULL}
};

int SendCmds(
	int 			Idx,
	int				MsgType,
	const char *	Cgi,
	void *			Params
){
//	Log3("CCCCCCCCCCCCall SendCmds:[%08X].\n",MsgType);

	int i = 0;
	for(i = 0;hACC[i].CmdCall != NULL;i++){
		if(MsgType == hACC[i].CmdType){

			Log3("app cmd send IOCTRL cmd:[%03X].",hACC[i].CmdType);
			
			return hACC[i].CmdCall(
				Idx,
				hACC[i].CmdType,
				Cgi,
				Params);
		}
	}

	return -1;
}
