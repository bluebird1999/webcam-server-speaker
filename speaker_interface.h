/*
 * player_interface.h
 *
 *  Created on: Sep 27, 2020
 *      Author: lijunxin
 */
#ifndef SERVER_SPEAKER_INTERFACE_H_
#define SERVER_SPEAKER_INTERFACE_H_

/*
 * header
 */
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"

/*
 * define
 */
#define		SERVER_SPEAKER_VERSION_STRING		"alpha-3.0.6"

#define		MSG_SPEAKER_BASE					(SERVER_SPEAKER<<16)
#define		MSG_SPEAKER_SIGINT					MSG_SPEAKER_BASE | 0x0000
#define		MSG_SPEAKER_SIGINT_ACK				MSG_SPEAKER_BASE | 0x1000
#define		MSG_SPEAKER_SIGTERM					MSG_SPEAKER_BASE | 0x0001
#define		MSG_SPEAKER_EXIT					MSG_SPEAKER_BASE | 0X0002
#define 	MSG_SPEAKER_CTL_PLAY				MSG_SPEAKER_BASE | 0x0003
#define 	MSG_SPEAKER_CTL_PLAY_ACK			MSG_SPEAKER_BASE | 0x1003
#define 	MSG_SPEAKER_CTL_DATA				MSG_SPEAKER_BASE | 0x0004
#define 	MSG_SPEAKER_CTL_DATA_ACK			MSG_SPEAKER_BASE | 0x1004
#define 	MSG_SPEAKER_PROPERTY_GET			MSG_SPEAKER_BASE | 0x0005
#define 	MSG_SPEAKER_PROPERTY_GET_ACK		MSG_SPEAKER_BASE | 0x1005



#define     SPEAKER_CTL_INTERCOM_START      	0x0030
#define     SPEAKER_CTL_INTERCOM_STOP       	0x0040
#define     SPEAKER_CTL_INTERCOM_DATA        	0x0050

#define     SPEAKER_CTL_DEV_START_FINISH        0x0060
#define     SPEAKER_CTL_ZBAR_SCAN_SUCCEED       0x0070
#define     SPEAKER_CTL_WIFI_CONNECT            0x0080
#define     SPEAKER_CTL_INTERNET_CONNECT_DEFEAT	0x0081
#define     SPEAKER_CTL_ZBAR_SCAN	            0x0090
#define     SPEAKER_CTL_INSTALLING		        0x0091
#define     SPEAKER_CTL_INSTALLEND				0x0092
#define     SPEAKER_CTL_INSTALLFAILED		    0x0093
#define     SPEAKER_CTL_RESET				    0x0094
#define 	SPEAKER_PLAYBACK_CHN_NUM			0x0010

/*
 * structure
 */
/*
typedef struct speaker_iot_config_t {
	//NULL
} speaker_iot_config_t;
*/
/*
 * function
 */
int server_speaker_start(void);
int server_speaker_message(message_t *msg);

#endif
