/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   play_audio.c
 * Author: ljx
 *
 * Created on September 27, 2020, 5:14 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rts_queue.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include "rtsavdef.h"
#include <rts_errno.h>
#include "../../tools/tools_interface.h"
#include "play_audio.h"
/*
 * 
 */

static void recycle_buffer(void *master, struct rts_av_buffer *buffer);
static int test_server(struct __server_info *info);
static int g_exit;

void stop_server(struct __server_info *info)
{
    g_exit = 0;
    
    rts_av_stop_send(info->decode_ch);

    rts_av_disable_chn(info->decode_ch);
    rts_av_disable_chn(info->dtom_resample_ch);
    rts_av_disable_chn(info->mixer_ch);
    rts_av_disable_chn(info->playback_ch);

    RTS_SAFE_CLOSE(info->decode_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->dtom_resample_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->mixer_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->playback_ch, rts_av_destroy_chn);

    rts_av_set_buffer_callback(info->buffers, NULL, NULL);
}

int clear_server_info(struct __server_info *info)
{

    RTS_ASSERT(info);

    rts_av_set_buffer_callback(info->buffers, NULL, NULL);

    if (info->idles) {
        rts_queue_clear(info->idles,
                        (cleanup_item_func)rts_av_put_buffer);
        RTS_SAFE_RELEASE(info->idles, rts_queue_destroy);
    }


    RTS_SAFE_RELEASE(info->buffers, rts_av_delete_buffer);

    return 0;
}

static int test_server(struct __server_info *info)
{
    struct rts_av_buffer *buffer = NULL;

    RTS_ASSERT(info);

    if (!rts_queue_empty(info->idles)) {
        buffer = rts_queue_pop(info->idles);
        rts_av_send(info->decode_ch, buffer);
        RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);
    }

    return 0;
}

int play_audio(char *path)
{
    int ret;
    struct __server_info info;
    int finish = 0;

    if(access(path, R_OK))
    {
        log_err("can not find %s\n", path);
        return -1;
    }
    
    memset(&info, 0, sizeof(info));
    ret = init_server_info(&info);
    if (ret) {
        log_err("init_server_info fail, ret = %d\n", ret);
        goto exit;
    }

    info.fp = fopen(path, "rb");
    if (!info.fp) {
        log_err("fail to open %s: %s\n", path, strerror(errno));
        ret = -1;
        goto exit1;
    }
    //第一次，跳过文件头
    fseek(info.fp, 0, SEEK_SET);
    //fseek(info.fp, 0x3a, SEEK_SET);

    ret = init_play_audio(&info);
    if (ret) {
        log_err("init_play_audio fail, ret = %d\n", ret);
        goto exit2;
    }

    usleep(1000);
    while (1) {
        test_server(&info);

        if (g_exit == 1 && feof(info.fp) && rts_av_is_idle(info.playback_ch)) {
            finish++;
        }

        if (finish >= 30) {
            usleep(1000 * 1000 * 1);
            log_info("finish\n");
            break;
        }
//        if(g_exit == 1 && rts_av_is_idle(info.playback_ch) && feof(info.fp)) {
//            break;
//        }
        usleep(1000);
    }

    stop_server(&info);

exit2:
    RTS_SAFE_RELEASE(info.fp, fclose);
exit1:
    clear_server_info(&info);
exit:
    return ret;
}


void recycle_buffer(void *master, struct rts_av_buffer *buffer)
{
    struct __server_info *info = master;
    int ret;

    if (g_exit)
        return;

    if (!info || !info->fp)
        return;

    buffer->bytesused = 0;
    ret = fread(buffer->vm_addr, 1, info->chunk_bytes, info->fp);
    if (ret == 0)
    {
        g_exit = 1;
    } else {
        buffer->bytesused = ret;
        buffer->timestamp = 0;
        rts_queue_push_back(info->idles, rts_av_get_buffer(buffer));        
    }
}

int init_server_info(struct __server_info *info)
{
    int ret;

    RTS_ASSERT(info);

    info->decode_ch = -1;
    info->dtom_resample_ch = -1;
    info->mixer_ch = -1;
    info->playback_ch = -1;
    snprintf(info->cfg_p.dev_node, sizeof(info->cfg_p.dev_node), "hw:0,1");
    info->cfg_p.rate = 16000;
    info->cfg_p.format = 16;
    info->cfg_p.channels = 2;

    info->chunk_bytes = 512;

    info->idles = rts_queue_init();
    if (!info->idles)
    {
        log_err("rts_queue_init failed\n");
        return -1;
    }

    info->buffers = rts_av_new_buffer(info->chunk_bytes);
    if (!info->buffers) {
        log_err("rts_av_new_buffer failed\n");
        ret = -1;
        goto exit;
    }

    return 0;
exit:
    if (info->idles) {
        rts_queue_clear(info->idles,
                        (cleanup_item_func)rts_av_put_buffer);
        RTS_SAFE_RELEASE(info->idles, rts_queue_destroy);
    }
    return ret;
}

int init_play_audio(struct __server_info *info)
{
    int ret;
    struct rts_av_profile profile;

    RTS_ASSERT(info);

    info->decode_ch = rts_av_create_audio_decode_chn();
    if (RTS_IS_ERR(info->decode_ch)) {
        log_err("rts_av_create_audio_decode_chn failed\n");
        return -1;
    }

    rts_av_get_profile(info->decode_ch, &profile);
    profile.fmt = RTS_A_FMT_ALAW;
    ret = rts_av_set_profile(info->decode_ch, &profile);
    if (RTS_IS_ERR(ret)) {
        log_err("rts_av_get_profile fail, ret = %d\n", ret);
        goto exit;
    }

    info->dtom_resample_ch = rts_av_create_audio_resample_chn(
                            info->cfg_p.rate, info->cfg_p.format,
                            info->cfg_p.channels);
    if (RTS_IS_ERR(info->dtom_resample_ch)) {
        ret = info->dtom_resample_ch;
        log_err("rts_av_create_audio_resample_chn fail, ret = %d\n", ret);
        goto exit;
    }

    info->mixer_ch = rts_av_create_audio_mixer_chn();
    if (RTS_IS_ERR(info->mixer_ch)) {
        log_err("rts_av_create_audio_mixer_chn failed");
        ret = info->mixer_ch;
        goto exit;
    }

    info->playback_ch = rts_av_create_audio_playback_chn(&info->cfg_p);
    if (RTS_IS_ERR(info->playback_ch)) {
        ret = info->playback_ch;
        log_err("rts_av_create_audio_playback_chn failed\n");
        goto exit;
    }

    ret = rts_av_bind(info->decode_ch, info->dtom_resample_ch);
    if (ret) {
        log_err("fail to bind decode and resample, ret = %d\n", ret);
        goto exit;
    }

    ret = rts_av_bind(info->dtom_resample_ch, info->mixer_ch);
    if (ret) {
        log_err("fail to bind resample and mixer, ret = %d\n", ret);
        goto exit;
    }

    ret = rts_av_bind(info->mixer_ch, info->playback_ch);
    if (ret) {
        log_err("fail to bind mixer and playback, ret = %d\n", ret);
        goto exit;
    }

    rts_av_enable_chn(info->decode_ch);
    rts_av_enable_chn(info->dtom_resample_ch);
    rts_av_enable_chn(info->mixer_ch);
    rts_av_enable_chn(info->playback_ch);

    rts_av_start_send(info->decode_ch);

    struct rts_av_buffer *buffer = rts_av_get_buffer(info->buffers);
    rts_av_set_buffer_callback(buffer, info, recycle_buffer);
    RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

    return 0;

exit:
    RTS_SAFE_CLOSE(info->decode_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->dtom_resample_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->mixer_ch, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(info->playback_ch, rts_av_destroy_chn);
    
    return ret;
}
