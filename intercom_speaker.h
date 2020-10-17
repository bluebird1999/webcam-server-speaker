#ifndef __INTERCOM_SPEAKER_H
#define __INTERCOM_SPEAKER_H

int intercom_speak(char *buffer, unsigned int buffer_size);
int intercom_stop(void);
int intercom_start(void);
void intercom_init(void);


#endif
