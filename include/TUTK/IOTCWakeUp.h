/**====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
*
* IOTCWakeUp.h
*
* Copyright (c) by TUTK Co.LTD. All Rights Reserved.
*
*
* \brief       The delaration of IOTC Wakeup api
*
*
* \description Due to power consumption concern, device will enter sleep mode and all
*              IOTC module will be disconnected. It needs a way to keep login and wake
*              IOTC up
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

#ifndef __IOTC_WAKEUP_H__
#define __IOTC_WAKEUP_H__


#ifdef _WIN32
	#if defined P2PAPI_EXPORTS
		#define P2PAPI_API __declspec(dllexport)
	#else
		#define P2PAPI_API __declspec(dllimport)
	#endif
#else
	#define P2PAPI_API
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _IOTC_WakeUpData
{
    unsigned long 		ulLoginAddr;		   // Login Address
    unsigned short		usLoginPort;		   // Login Port
    size_t              nLoginPacketLength;    // Login packet length
    char		        *pszLoginPacket;       // Login packet data buffer
    unsigned int        nLoginInterval;        // Send login time interval
    size_t              nWakeupPatternLength;  // WakeUp pattern length
    char		        *pszWakeupPattern;     // WakeUp pattern
} IOTC_WakeUpData;


/**
 * \brief Initial IOTC WakeUp feature
 *
 * \param None
 *
 * \return None
 *
 * \attention "Only" Device needs to use this API.
 *            Must invoke this API before using IOTC_WakeUp_Get_KeepAlivePacket() API
 */
P2PAPI_API void IOTC_WakeUp_Init();


/**
 * \brief Get IOTC keep alive data
 *
 * \param pData[out] pointer to IOTC_WakeUpData structure
 * \param pDataCnt[out]	IOTC_WakeUpData data structure count
 *
 * \return #IOTC_ER_NoERROR if device gets keep alive packets successfully
 * \return Error code if return value < 0
 *         - #IOTC_ER_TCP_NOT_SUPPORT  Device can't get keep alive data on TCP mode
 *         - #IOTC_ER_WAKEUP_NOT_INITIALIZED IOTC_WakeUp_Init() isn't called before using IOTC_WakeUp_Get_KeepAlivePacket()
 *
 * \attention "Only" Device needs to use this API.
 */
P2PAPI_API int IOTC_WakeUp_Get_KeepAlivePacket(IOTC_WakeUpData **pData, unsigned int *pDataCnt);


/**
 * \brief DeInitial IOTC WakeUp feature
 *
 * \param pData[in]	pointer to IOTC_WakeUpData structure
 *
 * \return None
 *
 * \attention "Only" Device needs to use this API.
 *            DeInit wake up feature and free all memory allocated by IOTC.
 *            It will cause memory leak if doesn't use this function at the
 *            end of wake up feature.
 */
P2PAPI_API void IOTC_WakeUp_DeInit(IOTC_WakeUpData *pData);


/**
 * \brief Send IOTC WakeUp request to wake device up
 *
 * \param pcszUID[in] The UID of a device that client wants to connect
 *
 * \return Error code if return value < 0
 *			- #IOTC_ER_NOT_SUPPORT Feature is not supported on current UID.
 *
 * \attention "Only" Client needs to use this API.
 */
P2PAPI_API int IOTC_WakeUp_WakeDevice(const char *pcszUID);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif // __IOTC_WAKEUP_H__
