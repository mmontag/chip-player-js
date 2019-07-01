#ifndef __AUDIOSTREAM_H__
#define __AUDIOSTREAM_H__

#ifdef __cplusplus
extern "C"
{
#endif

// Audio Drivers
/*
#define AUDDRV_WAVEWRITE

#ifdef _WIN32

#define AUDDRV_WINMM
#define AUDDRV_DSOUND
#define AUDDRV_XAUD2
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define AUDDRV_WASAPI	// no WASAPI for MS VC6 or MinGW
#endif

#elif defined(APPLE)

#define AUDDRV_CA

#else

#define AUDDRV_OSS
//#define AUDDRV_SADA
#define AUDDRV_ALSA
#define AUDDRV_LIBAO
#define AUDDRV_PULSE

#endif
*/

#include "AudioStructs.h"

/**
 * @brief Initializes the audio output system and loads audio drivers.
 *
 * @return error code. 0 = success, see AERR constants
 */
UINT8 Audio_Init(void);
/**
 * @brief Deinitializes the audio output system.
 *
 * @return error code. 0 = success, see AERR constants
 */
UINT8 Audio_Deinit(void);
/**
 * @brief Retrieve the number of loaded audio drivers.
 *
 * @return number of loaded audio drivers
 */
UINT32 Audio_GetDriverCount(void);
/**
 * @brief Retrieve information about a certain audio driver.
 *
 * @param drvID ID of the audio driver
 * @param retDrvInfo buffer for returning driver information
 * @return error code. 0 = success, see AERR constants
 */
UINT8 Audio_GetDriverInfo(UINT32 drvID, AUDDRV_INFO** retDrvInfo);

/**
 * @brief Creates an audio driver instance and initializes it.
 *
 * @param drvID ID of the audio driver
 * @param retDrvStruct buffer for returning the audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Init(UINT32 drvID, void** retDrvStruct);
/**
 * @brief Deinitializes and destroys an audio driver instance.
 *
 * @param drvStruct pointer to audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Deinit(void** drvStruct);
/**
 * @brief Retrieve the list of devices supported by an audio driver.
 *
 * @param drvStruct audio driver instance
 * @return pointer to a list of supported devices
 */
const AUDIO_DEV_LIST* AudioDrv_GetDeviceList(void* drvStruct);
/**
 * @brief Retrieve the default configuration for an audio driver.
 *
 * @param drvStruct audio driver instance
 * @return pointer to the default configuration
 */
AUDIO_OPTS* AudioDrv_GetOptions(void* drvStruct);
/**
 * @brief Retrieve an audio driver's data pointer for use with driver-specific calls.
 *
 * @param drvStruct audio driver instance
 * @return audio driver data pointer
 */
void* AudioDrv_GetDrvData(void* drvStruct);

/**
 * @brief Open an audio device and start an audio stream on that device.
 *
 * @param drvStruct audio driver instance
 * @param devID ID of the audio driver's device to be used
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Start(void* drvStruct, UINT32 devID);
/**
 * @brief Stops the audio stream and close the device.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Stop(void* drvStruct);
/**
 * @brief Pause the audio stream.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Pause(void* drvStruct);
/**
 * @brief Resume the audio stream.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_Resume(void* drvStruct);

/**
 * @brief Sets a callback function that is called whenever a buffer is free.
 *
 * @param drvStruct audio driver instance
 * @param FillBufCallback address of callback function
 * @param userParam pointer to user data
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_SetCallback(void* drvStruct, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
/**
 * @brief Adds another audio driver instance to data forwarding, so it will receive a copy of all audio data.
 *
 * @note Using Data Forwarding, you can tell the audio system to send a copy of all
 *       data the audio driver receives to one or multiple other drivers.
 *       This can be used e.g. to log all data that is played.
 *
 * @param drvStruct audio driver instance
 * @param destDrvStruct audio driver instance to be added to data forwarding
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_DataForward_Add(void* drvStruct, const void* destDrvStruct);
/**
 * @brief Removes an audio driver instance from data forwarding.
 *
 * @param drvStruct audio driver instance
 * @param destDrvStruct audio driver instance to be removed from data forwarding
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_DataForward_Remove(void* drvStruct, const void* destDrvStruct);
/**
 * @brief Remove all audio driver instances from data forwarding.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see AERR constants
 */
UINT8 AudioDrv_DataForward_RemoveAll(void* drvStruct);

/**
 * @brief Returns the maximum number of bytes that can be written using AudioDrv_WriteData().
 * @note Only valid after calling AudioDrv_Start().
 *
 * @param drvStruct audio driver instance
 * @return current buffer size in bytes
 */
UINT32 AudioDrv_GetBufferSize(void* drvStruct);
/**
 * @brief Checks whether you can send more data to the audio driver or not.
 *
 * @param drvStruct audio driver instance
 * @return AERR_OK: ready for more data, AERR_BUSY: busy playing previous data, see AERR constants for more error codes
 */
UINT8 AudioDrv_IsBusy(void* drvStruct);
/**
 * @brief Sends data to the audio driver in order to be played.
 *
 * @param drvStruct audio driver instance
 * @param dataSize size of the sample data in bytes
 * @param data sample data to be sent
 * @return latency in milliseconds
 */
UINT8 AudioDrv_WriteData(void* drvStruct, UINT32 dataSize, void* data);
/**
 * @brief Returns the current latency of the audio device in milliseconds.
 *
 * @param drvStruct audio driver instance
 * @return latency in milliseconds
 */
UINT32 AudioDrv_GetLatency(void* drvStruct);

#ifdef __cplusplus
}
#endif

#endif	// __AUDIOSTREAM_H__
