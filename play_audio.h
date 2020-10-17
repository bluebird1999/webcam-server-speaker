/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   play_audio.h
 * Author: ljx
 *
 * Created on September 27, 2020, 5:15 PM
 */
#ifndef SERVER_PLAYER_AUDIO_H_
#define SERVER_PLAYER_AUDIO_H_

#include "rtsavdef.h"
#include <rts_queue.h>

struct __server_info {
    int playback_ch;
    int mixer_ch;
    int decode_ch;
    int dtom_resample_ch;
    FILE *fp;
    uint32_t chunk_bytes;
    struct rts_audio_attr cfg_p;
    struct rts_av_buffer *buffers;
    Queue idles;
    int finish;
};


int play_audio(char *path);

int init_play_audio(struct __server_info *info);
int init_server_info(struct __server_info *info);

int clear_server_info(struct __server_info *info);
void stop_server(struct __server_info *info);

#endif /* SERVER_PLAYER_AUDIO_H_ */
