
#ifndef PULSE_H
#define PULSE_H

/**
 * Transform pcm data to user space. 
 * data: pointer to pcm data.
 * len : max length of this block size
*/
typedef int (*callback_get_pcm)(unsigned char *data, unsigned int len);

struct pulseUnit
{
    int rate;
    int channels;
    int format;
    int writefile;
    char filename[32];
    int have_callback;  //1-have callback; 0-not
    int max_block;
    callback_get_pcm cb_pcm_data;

};
/**
 * 
*/
int pulse_init(struct pulseUnit *pObj);

/**
 * 
*/
int pulse_loop_in();


#endif