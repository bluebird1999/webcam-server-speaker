#ifndef __INTERCOM_SPEAKER_H
#define __INTERCOM_SPEAKER_H

typedef struct playback {
    struct rts_audio_attr attr;
    int playback;
    int decode;
    int resample;
    int mixer;
    int is_intercom_start;
}Playback;

Playback myplayback;

int intercom_speak(char *buffer, unsigned int buffer_size);
int intercom_stop(void);
int intercom_start(void);
void intercom_init(void);


#endif
