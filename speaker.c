/*
 * player.c
 *
 *  Created on: Sep 13, 2020
 *      Author: lijunxin
 */

/*
 * header
 */
//system header
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <unistd.h>
#include <string.h>
//program header
#include "../../manager/global_interface.h"
#include "../../manager/manager_interface.h"
#include "../../server/realtek/realtek_interface.h"
#include "../../tools/tools_interface.h"
#include "../../server/audio/config.h"
#include "../../server/miio/miio_interface.h"
#include "../../server/miss/miss_interface.h"
#include "../../server/audio/audio_interface.h"
//server header
#include "speaker.h"
#include "speaker_interface.h"
#include "play_audio.h"
#include "intercom_speaker.h"


/*
 * #define
 */
#define DEV_START_FINISH 			"/opt/qcy/audio_resource/dev_start_finish.alaw"
#define DEV_START_ING				"/opt/qcy/audio_resource/dev_starting.alaw"
#define WIFI_CONNECT_SUCCEED 		"/opt/qcy/audio_resource/wifi_connect_success.alaw"
#define ZBAR_SCAN_SUCCEED 			"/opt/qcy/audio_resource/scan_zbar_success.alaw"
#define ZBAR_SCAN					"/opt/qcy/audio_resource/wait_connect.alaw"
#define INSTALLING					"/opt/qcy/audio_resource/begin_update.alaw"
#define INSTALLEND					"/opt/qcy/audio_resource/success_upgrade.alaw"
#define INSTALLFAILED				"/opt/qcy/audio_resource/upgrade_failed.alaw"
#define RESET_SUCCESS				"/opt/qcy/audio_resource/reset_success.alaw"

/*
 * static
 */
static int intercom_flag = 0;

//variable
static 	message_buffer_t	message;
static 	server_info_t 		info;
static server_name_t server_name_tt[] = {
		{0, "config"},
		{1, "device"},
		{2, "kernel"},
		{3, "realtek"},
		{4, "miio"},
		{5, "miss"},
		{6, "micloud"},
		{7, "video"},
		{8, "audio"},
		{9, "recorder"},
		{10, "player"},
		{11, "speaker"},
		{12, "video2"},
		{13, "scanner"},
		{32, "manager"},
};

//function
//common
static void *server_func(void *arg);
static int server_message_proc(void);
static int server_none(void);
static int server_wait(void);
static int server_setup(void);
static int server_idle(void);
static int server_start(void);
static int server_run(void);
static int server_stop(void);
static int server_restart(void);
static int server_error(void);
static int server_release(void);
static int server_get_status(int type);
static int server_set_status(int type, int st);
static void server_thread_termination(int arg);
static int send_iot_ack(message_t *org_msg, message_t *msg, int id, int receiver, int result, void *arg, int size);
static int send_message(int receiver, message_t *msg);
//specific
static int play(char *path);
static char* get_string_name(int i);
/*
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 * %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 */
static int play(char *path)
{
    int ret;
    ret = play_audio(path);
    if(ret)
        log_err("play_audio failed");
    return ret;
}

static void server_thread_termination(int arg)
{
    message_t msg;
    /********message body********/
    msg_init(&msg);
    msg.message = MSG_SPEAKER_SIGINT;
    msg.sender = msg.receiver = SERVER_SPEAKER;
    /****************************/
    manager_message(&msg);
}

static char* get_string_name(int i)
{
	char *ret = NULL;

	if(i == SERVER_MANAGER)
		ret = server_name_tt[sizeof(server_name_tt)/sizeof(server_name_t) - 1].name;
	else
		ret = server_name_tt[i].name;

	return ret;
}

static int server_release(void)
{
    int ret = 0;

    ret = intercom_stop();

    msg_buffer_release(&message);

    return ret;
}

static int server_set_status(int type, int st)
{
    int ret = -1;
    
    ret = pthread_rwlock_wrlock(&info.lock);
    if(ret) {
        log_err("add lock fail, ret = %d", ret);
        return ret;
    }
    
    if(type == STATUS_TYPE_STATUS) {
        info.status = st;
    }
    else if(type == STATUS_TYPE_EXIT) {
        info.exit = st;
    }

    ret = pthread_rwlock_unlock(&info.lock);
    if (ret) {
        log_err("add unlock fail, ret = %d", ret);
    }
    
    return ret;
}

static int send_iot_ack(message_t *org_msg, message_t *msg, int id, int receiver, int result, void *arg, int size)
{
	int ret = 0;
    /********message body********/
	msg_init(msg);
	memcpy(&(msg->arg_pass), &(org_msg->arg_pass),sizeof(message_arg_t));
	msg->message = id | 0x1000;
	msg->sender = msg->receiver = SERVER_SPEAKER;
	msg->result = result;
	msg->arg = arg;
	msg->arg_size = size;
	ret = send_message(receiver, msg);
	/***************************/
	return ret;
}

static int send_message(int receiver, message_t *msg)
{
	int st;
	switch(receiver) {
	case SERVER_CONFIG:
//		st = server_config_message(msg);
		break;
	case SERVER_DEVICE:
		break;
	case SERVER_KERNEL:
		break;
	case SERVER_REALTEK:
		break;
	case SERVER_MIIO:
		st = server_miio_message(msg);
		break;
	case SERVER_MISS:
		st = server_miss_message(msg);
		break;
	case SERVER_MICLOUD:
		break;
	case SERVER_AUDIO:
		st = server_audio_message(msg);
		break;
	case SERVER_RECORDER:
		break;
	case SERVER_PLAYER:
		break;
	case SERVER_MANAGER:
		st = manager_message(msg);
		break;
	}
	return st;
}

static int server_get_status(int type)
{
    int st = 0;
    int ret;

    ret = pthread_rwlock_wrlock(&info.lock);
    if(ret) {
        log_err("add lock fail, ret = %d", ret);
        return ret;
    }

    if(type == STATUS_TYPE_STATUS) {
        st = info.status;
    }
    else if(type== STATUS_TYPE_EXIT) {
            st = info.exit;
    }

    ret = pthread_rwlock_unlock(&info.lock);
    if (ret) {
        log_err("add unlock fail, ret = %d", ret);
    }

    return st;
}

static int server_message_proc(void)
{
    int ret = 0, ret1 = 0;
    message_t msg;
    message_t send_msg;
    msg_init(&msg);
    msg_init(&send_msg);
    ret = pthread_rwlock_wrlock(&message.lock);
    if(ret) {
        log_err("add message lock fail, ret = %d\n", ret);
        return ret;
    }
    ret = msg_buffer_pop(&message, &msg);
    ret1 = pthread_rwlock_unlock(&message.lock);
    if (ret1)
        log_err("add message unlock fail, ret = %d\n", ret1);
    if (ret == -1)
        return -1;
    else if( ret == 1)
    {
    	return 0;
    }
    switch(msg.message){
        case MSG_MANAGER_EXIT:
            server_set_status(STATUS_TYPE_EXIT,1);
            break;
        case MSG_MANAGER_TIMER_ACK:
            ((HANDLER)(msg.arg))();
            break;
        case MSG_SPEAKER_CTL_PLAY:
        	if( msg.arg_in.cat == SPEAKER_CTL_DEV_START_FINISH ) {
				ret = play(DEV_START_FINISH);
//				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//						NULL, 0);
			} else if( msg.arg_in.cat == SPEAKER_CTL_ZBAR_SCAN_SUCCEED ) {
				ret = play(ZBAR_SCAN_SUCCEED);
//				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//						NULL, 0);
			} else if( msg.arg_in.cat == SPEAKER_CTL_WIFI_CONNECT ) {
				ret = play(WIFI_CONNECT_SUCCEED);
//				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//						NULL, 0);
			} else if( msg.arg_in.cat == SPEAKER_CTL_ZBAR_SCAN ) {
				ret = play(ZBAR_SCAN);
//				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//						NULL, 0);
			} else if( msg.arg_in.cat == SPEAKER_CTL_INTERCOM_START ) {
				ret = intercom_start();
				intercom_flag = 1;
				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
						NULL, 0);
			} else if( msg.arg_in.cat == SPEAKER_CTL_INTERCOM_STOP ) {
				ret = intercom_stop();
				intercom_flag = 0;
				send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
						NULL, 0);
			}

			else if( msg.arg_in.cat == SPEAKER_CTL_INSTALLING ) {
							ret = play(INSTALLING);
//							send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//									NULL, 0);
			}
			else if( msg.arg_in.cat == SPEAKER_CTL_INSTALLEND ) {
							ret = play(INSTALLEND);
//							send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//									NULL, 0);
			}
			else if( msg.arg_in.cat == SPEAKER_CTL_INSTALLFAILED ) {
							ret = play(INSTALLFAILED);
//							send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//									NULL, 0);
			}

			else if( msg.arg_in.cat == SPEAKER_CTL_RESET ) {
							ret = play(RESET_SUCCESS);
//							send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//									NULL, 0);
			}
			else if( msg.arg_in.cat == SPEAKER_CTL_DEV_START_ING ) {
							ret = play(DEV_START_ING);
//							send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_PLAY_ACK, msg.receiver, ret,
//									NULL, 0);
			}
            break;
        case MSG_SPEAKER_CTL_DATA:
        	if( msg.arg_in.cat == SPEAKER_CTL_INTERCOM_DATA ) {
        		if(msg.arg) {
					intercom_speak(msg.arg, msg.arg_size);
				}
				//send_iot_ack(&msg, &send_msg, MSG_SPEAKER_CTL_DATA_ACK, msg.receiver, ret,
				//		NULL, 0);
			}
            break;
        default:
        	log_err("not processed message = %d", msg.message);
        	break;
    }
    
    msg_free(&msg);
    
    return ret;
}


/*
 * State Machine
 */
static int server_none(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS, STATUS_WAIT);
    return ret;
}

static int server_wait(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS, STATUS_SETUP);
    return ret;
}

static int server_setup(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS, STATUS_IDLE);
    intercom_init();
    
    return ret;
}

static int server_idle(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS, STATUS_START);
    return ret;
}

static int server_start(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS, STATUS_RUN);
    return ret;
}

static int server_run(void)
{
    int ret = 0;
    if(server_message_proc() != 0) {
        server_set_status(STATUS_TYPE_STATUS, STATUS_STOP);
    }
    return ret;
}

static int server_stop(void)
{
    int ret = 0;
    server_set_status(STATUS_TYPE_STATUS,STATUS_IDLE);
    return ret;
}

static int server_restart(void)
{
    int ret = 0;
    
    server_release();
    server_set_status(STATUS_TYPE_STATUS,STATUS_WAIT);
    
    return ret;
}

static int server_error(void)
{
    int ret = 0;
    
    server_release();
    log_err("!!!!!!!!error in speaker!!!!!!!!");
    
    return ret;
}

static void *server_func(void* arg)
{
    signal(SIGINT, server_thread_termination);
    signal(SIGTERM, server_thread_termination);
    
    misc_set_thread_name("server_speaker");
    pthread_detach(pthread_self());
	while( !info.exit ) {
	switch(info.status){
		case STATUS_NONE:
			server_none();
			break;
		case STATUS_WAIT:
			server_wait();
			break;
		case STATUS_SETUP:
			server_setup();
			break;
		case STATUS_IDLE:
			server_idle();
			break;
		case STATUS_START:
			server_start();
			break;
		case STATUS_RUN:
			server_run();
			break;
		case STATUS_STOP:
			server_stop();
			break;
		case STATUS_RESTART:
			server_restart();
			break;
		case STATUS_ERROR:
			server_error();
			break;
		}
		//usleep(100 * 1000);//100ms
		if(!intercom_flag)
			usleep(100 * 1000);
	}
    
    server_release();
    
    message_t msg;
    /********message body********/
    msg_init(&msg);
    msg.message = MSG_MANAGER_EXIT_ACK;
    msg.sender = SERVER_SPEAKER;
    /***************************/
    manager_message(&msg);
    
    log_info("-----------thread exit: server_speaker-----------");
    pthread_exit(0);
}

/*
 * external interface
 */
int server_speaker_start(void)
{
    int ret = -1;
    if( message.init == 0) {
        msg_buffer_init(&message, MSG_BUFFER_OVERFLOW_NO);
    }
    
    memset(&info, 0, sizeof(info));
    
    pthread_rwlock_init(&info.lock, NULL);
    ret = pthread_create(&info.id, NULL, server_func, NULL);
    if(ret != 0) {
        log_err("speaker server create error! ret = %d",ret);
        return ret;
    }
    else {
        log_err("speaker server create successful!");
        return 0;
    }
}

int server_speaker_message(message_t *msg)
{
	int ret = 0;
	if( server_get_status(STATUS_TYPE_STATUS)!= STATUS_RUN ) {
		log_qcy(DEBUG_SERIOUS, "speaker server is not ready!");
		return -1;
	}
	ret = pthread_rwlock_wrlock(&message.lock);
	if(ret)	{
		log_qcy(DEBUG_SERIOUS, "add message lock fail, ret = %d\n", ret);
		return ret;
	}
	ret = msg_buffer_push(&message, msg);
	log_qcy(DEBUG_VERBOSE, "push into the speaker message queue: sender=%s, message=%d, ret=%d", get_string_name(msg->sender), msg->message, ret);
	if( ret!=0 )
		log_qcy(DEBUG_SERIOUS, "message push in speaker error =%d", ret);
	ret = pthread_rwlock_unlock(&message.lock);
	if (ret)
		log_qcy(DEBUG_SERIOUS, "add message unlock fail, ret = %d\n", ret);
	return ret;
}
