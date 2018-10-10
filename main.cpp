//#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>
#include "pulse.h"
#include "memkit.h"
#include "list.h"
#include "aac_kit.h"

#define     MEM_BLOCK_SIZE      (10*1024)
#define     MEM_BLOCKS_NUM      (30)

#define     PA_CHANNELS     (2)
#define     PA_RATE         (44100)

struct MemKitHandle     g_pcmhandle;

struct list_head        g_pcm_list;
pthread_mutex_t         g_pcm_mtx;
pthread_cond_t          g_pcm_cond;

struct dataUnit
{
    struct list_head list;
    unsigned int len;
    struct MemPacket *pkt;
};

int cb_get_pcm(unsigned char *buf, unsigned int len)
{
    struct MemItorVec itor;
    struct MemPacket *pkt = NULL;
    struct dataUnit unit;
    unsigned int allsize = 0;
    int blk_len = 0;
    int thislen = 0;
    unsigned char *ptr = NULL;

    if(NULL != buf && len > 0)
    {
        INFO("Doing get pcm data buf:%p, len:%d!\n", buf, len);
    }
    allsize = len;

    pkt = mk_malloc(&g_pcmhandle, len, "pcm");

    mk_set_itor(pkt, &itor);
    ptr = buf;

    while(allsize > 0 && mk_next_entry(&itor, &blk_len))
    {
        thislen = blk_len > allsize ? allsize : blk_len;
        memcpy(itor.entry, ptr, thislen);
        ptr += thislen;
        allsize -= thislen;
        //itor.poffset
    }

    pthread_mutex_lock(&g_pcm_mtx);
    list_add_tail(&unit.list, &g_pcm_list);
    pthread_mutex_unlock(&g_pcm_mtx);
    pthread_cond_signal(&g_pcm_cond);
}

int main(int argc, char *argv[])
{
    printf("hello world!\n");
    int ret = 0;
    struct pulseUnit pu;

    INIT_LIST_HEAD(&g_pcm_list);
    pthread_mutex_init(&g_pcm_mtx, NULL);
    pthread_cond_init(&g_pcm_cond, NULL);

    memset(&pu, 0, sizeof(struct pulseUnit));
    pu.rate = PA_RATE;
    pu.channels = PA_CHANNELS;
    pu.format = PA_SAMPLE_S16LE;
    pu.writefile = 1;
    strcpy(pu.filename, "audio.pcm");
    pu.have_callback = 1;
    pu.cb_pcm_data = cb_get_pcm;
    
    ret = mk_handle_init(&g_pcmhandle, MEM_BLOCKS_NUM, MEM_BLOCK_SIZE);
    if(ret < 0)
    {
        INFO("Cannot do mem kit handle init !\n");
        exit(1);
    }
    aac_init(PA_RATE, PA_CHANNELS);
    pulse_init(&pu);

    



    pulse_loop_in();
    return 0;
}
