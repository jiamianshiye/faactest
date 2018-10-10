#include <stdio.h>
#include <stdint.h>
#include <faac.h>
#include "aac_kit.h"
#include "memkit.h"


static int g_inputsamples = 0;
static int g_maxoutbyts = 0; 
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
    pConfiguration = faacEncGetCurrentConfiguration(g_aacEncHdl); 
    pConfiguration->inputFormat = FAAC_INPUT_16BIT;
    pConfiguration->outputFormat = 1;
    pConfiguration->useTns = true;
    pConfiguration->useLfe = false;
    pConfiguration->aacObjectType = LOW;
    pConfiguration->mpegVersion = MPEG4;
    //pConfiguration->shortctl=SHORTCTL_NORMAL;
    pConfiguration->quantqual = 50;
    pConfiguration->bandWidth = 0;
    pConfiguration->bitRate = 0;

    if(faacEncSetConfiguration(g_aacEncHdl, pConfiguration) < 0)
    {
        ERROR("Cannot set configure for aac handle!\n");
        return -1;
    }

    return 0;
}

int aac_enc_pcm(unsigned char *buff, unsigned int buflen, unsigned char *out)
{
    int ret = faacEncEncode(g_aacEncHdl, (int *)buff, g_inputsamples, out, g_maxoutbyts);
    if(ret < 1)
    {
        INFO("Cannot encoder this frame of pcm data!\n");
        return -1;
    }
    return ret;
}
