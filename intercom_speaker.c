#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <getopt.h>

#include "../../manager/global_interface.h"
#include "../../tools/tools_interface.h"
#include "intercom_speaker.h"

static void recycle_buffer(void *master, struct rts_av_buffer *buffer)
{    
    RTS_SAFE_RELEASE(buffer, rts_av_delete_buffer);  
}

int intercom_speak(char *buffer, unsigned int buffer_size)
{
    struct rts_av_buffer *rts_buffer = NULL;

    if(myplayback.is_intercom_start == 0) {
        log_qcy(DEBUG_SERIOUS, "intercom already stop");
        return -1;
    }

    rts_buffer = rts_av_new_buffer(buffer_size);                
    if (!rts_buffer) {                     
        log_qcy(DEBUG_SERIOUS, "alloc buffer fail\n");
        return -1;
    }
    
    rts_av_set_buffer_callback(rts_buffer, &buffer_size, recycle_buffer);
    rts_av_get_buffer(rts_buffer);
    
    memcpy(rts_buffer->vm_addr, buffer, buffer_size);
    rts_buffer->bytesused = buffer_size;
    rts_buffer->timestamp = 0;
    rts_av_send(myplayback.decode, rts_buffer);
    
    RTS_SAFE_RELEASE(rts_buffer, rts_av_put_buffer);
    return 0;
}

int intercom_stop(void)
{
    if(myplayback.is_intercom_start == 0) {
        log_qcy(DEBUG_SERIOUS, "intercom already stop");
        return -1;
    }
    
    if(myplayback.decode > -1)
    	rts_av_stop_send(myplayback.decode);
    
    if(myplayback.playback > -1)
		rts_av_disable_chn(myplayback.playback);
    if(myplayback.resample > -1)
		rts_av_disable_chn(myplayback.resample);
    if(myplayback.decode > -1)
		rts_av_disable_chn(myplayback.decode);
    if(myplayback.mixer > -1)
		rts_av_disable_chn(myplayback.mixer);
    
    if(myplayback.playback > -1)
		rts_av_destroy_chn(myplayback.playback);
    if(myplayback.resample > -1)
		rts_av_destroy_chn(myplayback.resample);
    if(myplayback.decode > -1)
		rts_av_destroy_chn(myplayback.decode);
    if(myplayback.mixer > -1)
		rts_av_destroy_chn(myplayback.mixer);
    
    myplayback.playback = -1;
    myplayback.resample = -1;
    myplayback.decode = -1;
    myplayback.mixer = -1;
    
    myplayback.is_intercom_start = 0;
    
    return 0;
}

int intercom_start(void)
{
    int ret;
    struct rts_av_profile profile;
    
    if(myplayback.is_intercom_start == 1) {
        log_qcy(DEBUG_SERIOUS, "intercom already start");
        goto err_decode;
    }
    
    if(myplayback.decode < 0) {
        myplayback.decode = rts_av_create_audio_decode_chn();  
        if (RTS_IS_ERR(myplayback.decode)) {   
            log_qcy(DEBUG_SERIOUS, "create audio decode chn fail, ret = %d\n", myplayback.decode);
            ret = -1;
            goto err_decode;
        }
    }
    
    rts_av_get_profile(myplayback.decode, &profile);
    profile.fmt = RTS_A_FMT_ALAW;
    ret = rts_av_set_profile(myplayback.decode, &profile);    
    if (RTS_IS_ERR(ret)) {    
        log_qcy(DEBUG_SERIOUS, "set decode fail, ret = %d\n", ret);
        ret = -1;
        goto err_set_profile;
    }
    
    snprintf(myplayback.attr.dev_node, sizeof(myplayback.attr.dev_node), "hw:0,1");
    myplayback.attr.channels = 1;
    myplayback.attr.format = 16;
    myplayback.attr.rate = 8000;
    
    if(myplayback.resample < 0) {
        myplayback.resample = rts_av_create_audio_resample_chn(myplayback.attr.rate,
                myplayback.attr.format, myplayback.attr.channels);
        if (RTS_IS_ERR(myplayback.resample)) {   
            log_qcy(DEBUG_SERIOUS, "create audio resample chn fail, ret = %d\n",
                    myplayback.resample);
            ret = -1;
            goto err_resample;
        }
    }
    
    if(myplayback.mixer < 0) {
        myplayback.mixer = rts_av_create_audio_mixer_chn();
        if (RTS_IS_ERR(myplayback.mixer)) {
            log_qcy(DEBUG_SERIOUS, "rts_av_create_audio_mixer_chn failed, ret = %d", myplayback.mixer);
            ret = -1;
            goto err_mixer;
        }
    }
    
    if(myplayback.playback < 0) {
        myplayback.playback = rts_av_create_audio_playback_chn(&myplayback.attr); 
        if (RTS_IS_ERR(myplayback.playback)) {    
        	log_qcy(DEBUG_SERIOUS, "create audio playback chn fail, ret = %d",
                    myplayback.playback);       
            ret = -1;
            goto err_playback;
        }
    }
    
    ret = rts_av_bind(myplayback.decode, myplayback.resample);  
    if (RTS_IS_ERR(ret)) {     
    	log_qcy(DEBUG_SERIOUS, "bind decode and resample fail\n");
    	ret = -1;
        goto err_bind;
    }        
    
    ret = rts_av_bind(myplayback.resample, myplayback.mixer);  
    if (RTS_IS_ERR(ret)) {     
    	log_qcy(DEBUG_SERIOUS, "bind decode and resample fail\n");
    	ret = -1;
        goto err_bind;
    }
    
    ret = rts_av_bind(myplayback.mixer, myplayback.playback); 
    if (RTS_IS_ERR(ret)) {    
    	log_qcy(DEBUG_SERIOUS, "bind resample and playback fail\n");
    	ret = -1;
        goto err_bind;
    }
    
    rts_av_enable_chn(myplayback.decode);   
    rts_av_enable_chn(myplayback.resample);   
    rts_av_enable_chn(myplayback.mixer);
    rts_av_enable_chn(myplayback.playback);
    
    rts_av_start_send(myplayback.decode);
    
    myplayback.is_intercom_start = 1;
    
    return 0;

err_bind:
err_playback:
	rts_av_disable_chn(myplayback.playback);
	rts_av_destroy_chn(myplayback.playback);
	myplayback.playback = -1;

err_mixer:
	rts_av_disable_chn(myplayback.mixer);
	rts_av_destroy_chn(myplayback.mixer);
	myplayback.mixer = -1;

err_resample:
	rts_av_disable_chn(myplayback.resample);
	rts_av_destroy_chn(myplayback.resample);
	myplayback.resample = -1;

err_set_profile:
	rts_av_disable_chn(myplayback.decode);
	rts_av_destroy_chn(myplayback.decode);
	myplayback.decode = -1;

err_decode:
	return ret;

}

void intercom_init(void)
{
    memset(&myplayback, -1, sizeof(myplayback));
    myplayback.is_intercom_start = 0;
}
