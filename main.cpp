//#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>

#include <errno.h>

#include "pulse.h"
#include "memkit.h"
#include "list.h"
#include "aac_kit.h"

#define     MEM_BLOCK_SIZE      (10*1024)
#define     MEM_BLOCKS_NUM      (30)

#define     PA_CHANNELS         (2)
#define     PA_RATE             (44100)

struct MemKitHandle     g_pcmhandle;

struct list_head        g_pcm_list;
pthread_mutex_t         g_pcm_mtx;
pthread_cond_t          g_pcm_cond;



int cb_get_pcm(unsigned char *buf, unsigned int len)
{
    struct MemItorVec itor;
    struct MemPacket *pkt = NULL;
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
    list_add_tail(&pkt->list, &g_pcm_list);
    pthread_mutex_unlock(&g_pcm_mtx);
    pthread_cond_signal(&g_pcm_cond);
}


void *enc_pcm_func(void *arg)
{
    struct MemPacket *pkt = NULL;
    struct list_head *pos, *n;
    while(1)
    {
        pthread_mutex_lock(&g_pcm_mtx);
        while(list_empty(&g_pcm_list))
        {
            pthread_cond_wait(&g_pcm_cond, &g_pcm_mtx);
        }
        list_for_each_safe(pos, n, &g_pcm_list)
        {
            pkt = list_entry(pos, struct MemPacket, list);
            if(pkt == NULL)
            {
                ERROR("Cannot get entry of this packet!\n");
            }
            list_del(&pkt->list);
        }
        pthread_mutex_unlock(&g_pcm_mtx);
        


        INFO("Get a packet for faac!\n");

        mk_free(pkt);
    }

}

int main(int argc, char *argv[])
{
    printf("hello world!\n");
    int ret = 0;
    struct pulseUnit pu;
    pthread_t enc_pid;

    INIT_LIST_HEAD(&g_pcm_list);
    pthread_mutex_init(&g_pcm_mtx, NULL);
    pthread_cond_init(&g_pcm_cond, NULL);

    if(pthread_create(&enc_pid, NULL, enc_pcm_func, NULL) < 0)
    {
        ERROR("Cannot create faac encode thread:%s!\n", strerror(errno));
        return -1;
    }

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
