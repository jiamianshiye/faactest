#include <stdio.h>
#include <stdint.h>
#include <faac.h>
#include "aac_kit.h"
#include "memkit.h"


static int g_inputsamples = 0;
static int g_maxoutbyts = 0; 
unsigned int pcm_frame_bit = 16;
static faacEncHandle g_aacEncHdl;
static faacEncConfigurationPtr pConfiguration;

int aac_init(int rate, int channels)
{
    
    g_aacEncHdl = faacEncOpen(rate, channels, (long unsigned int*)&g_inputsamples, (long unsigned int *)&g_maxoutbyts);
    if(g_aacEncHdl == NULL)
    {
        ERROR("Wrong to open faac enc!\n");
        return -1;
    }
    INFO("input samples:%d , maxout :%d\n", g_inputsamples, g_maxoutbyts);
    pConfiguration = faacEncGetCurrentConfiguration(g_aacEncHdl); 
    pConfiguration->inputFormat = FAAC_INPUT_16BIT;
    pConfiguration->outputFormat = 1;
    //pConfiguration->useTns = 1;
    pConfiguration->useLfe = 0;
    pConfiguration->aacObjectType = LOW;
    pConfiguration->allowMidside = 0;
    //pConfiguration->shortctl=SHORTCTL_NORMAL;
    pConfiguration->bandWidth = 32000;
    pConfiguration->bitRate = 48000;

    if(faacEncSetConfiguration(g_aacEncHdl, pConfiguration) < 0)
    {
        ERROR("Cannot set configure for aac handle!\n");
        return -1;
    }

    return 0;
}

int aac_enc_pcm(unsigned char *pcmbuffer, unsigned int buflen, unsigned char *out)
{
    unsigned long inputsample = 0;
    inputsample = buflen /(pcm_frame_bit / 8);
    int ret = faacEncEncode(g_aacEncHdl, (int *)pcmbuffer, inputsample, out, g_maxoutbyts);
    if(ret < 1)
    {
        INFO("Cannot encoder this frame of pcm data!\n");
        return -1;
    }
    INFO("enc pcmsize:%d, samples:%d, aacsize:%d, maxoutbytes:%d!\n", buflen, inputsample, ret, g_maxoutbyts);
    return ret;
}

int aac_enc_close()
{
    faacEncClose(g_aacEncHdl);
}