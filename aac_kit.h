#ifndef AAC_KIT_H
#define AAC_KIT_H

int aac_init(int rate, int channels);

int aac_enc_pcm(unsigned char *buff, unsigned int buflen, unsigned char *out);

int aac_enc_close();
#endif