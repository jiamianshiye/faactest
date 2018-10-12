//#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "pulse.h"
#include "memkit.h"
#include "list.h"
#include "aac_kit.h"

#define     MEM_BLOCK_SIZE      (10*1024)
#define     MEM_BLOCKS_NUM      (30)

#define     PA_CHANNELS         (2)
#define     PA_RATE             (16000)

struct MemKitHandle     g_pcmhandle;

struct list_head        g_pcm_list;
pthread_mutex_t         g_pcm_mtx;
pthread_cond_t          g_pcm_cond;

#define     DEBUG       (1)

unsigned char    g_pcm_buf[100*1000];
int     g_pcm_off = 0;

int cb_get_pcm(unsigned char *buf, unsigned int len)
{
    struct MemItorVec itor;
    struct MemPacket *pkt = NULL;
    unsigned int allsize = 0;
    unsigned int blk_len = 0;
    unsigned int thislen = 0;
    unsigned char *ptr = NULL;
    char name[] = "pcm";
    int pcm_blk = 4096;
    unsigned char aacbuf[10*1024];
    int ret = 0;

    if(NULL != buf && len > 0)
    {
        INFO("Doing get pcm data buf:%p, len:%d!\n", buf, len);
    }


    ptr = &g_pcm_buf[g_pcm_off];
    memcpy(ptr, buf, len);
    g_pcm_off += len;
    if(g_pcm_off < pcm_blk)
    {
        return 0;
    }

    pkt = mk_malloc(&g_pcmhandle, pcm_blk, name);
    if(pkt == NULL)
    {
        usleep(10*1000);
        return -1;
    }

    mk_set_itor(pkt, &itor);
    ptr = g_pcm_buf;
    allsize = pcm_blk;

    //while(allsize > 0 && mk_next_entry(&itor, &blk_len))
    if(mk_next_entry(&itor, &blk_len) == 0)
    {
        thislen = blk_len > allsize ? allsize : blk_len;
        memcpy(itor.entry, ptr, thislen);
        ptr += thislen;
        allsize -= thislen;
        //itor.poffset
    }
    
    //int aaclen = aac_enc_pcm((unsigned char *)buf, (unsigned int)len, aacbuf);
    //INFO("aaclen : %d, pcm:%p, pcmlen:%d\n", aaclen,buf, len);

    pthread_mutex_lock(&g_pcm_mtx);
    list_add_tail(&pkt->list, &g_pcm_list);
    pthread_mutex_unlock(&g_pcm_mtx);
    pthread_cond_signal(&g_pcm_cond);

    //g_pcm_off -= pcm_blk;
    g_pcm_off = 0;
    if(g_pcm_off != 0)
    {
        memmove(g_pcm_buf, &g_pcm_buf[pcm_blk], g_pcm_off);
    }
    return 0;
}


void *enc_pcm_func(void *arg)
{
    struct MemPacket *pkt = NULL;
    struct list_head *pos, *n;
    struct MemItorVec itor;
    unsigned char aacbuf[100*2048];
    int aaclen = 0;
    unsigned int blocklen = 0;

#if DEBUG
    int times = 600;
    int aac_fd;
    int pcm_fd;

    aac_fd = open("audio.aac", O_RDWR | O_CREAT);
    if(aac_fd < 0)
    {
        ERROR("Cannot open audio.aac : %s!\n", strerror(errno));
        exit(1);
    }
    pcm_fd = open("audio.pcm", O_RDWR | O_CREAT);
    if(aac_fd < 0)
    {
        ERROR("Cannot open audio.aac : %s!\n", strerror(errno));
        exit(1);
    }
    while(times-- > 0)
#else
    while(1)
#endif    
    {
        memset(&itor, 0, sizeof(itor));
        memset(aacbuf, 0, 100*1024);

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

        INFO("Get a packet for faac:pkt size:%d!\n", pkt->total_size);

        mk_set_itor(pkt, &itor);
        mk_next_entry(&itor, &blocklen);
        write(pcm_fd, itor.entry, pkt->total_size);
        aaclen = aac_enc_pcm(itor.entry, pkt->total_size, aacbuf);
        if(aaclen <= 0)
        {
            ERROR("Cannot enc this pcm data!\n");
        }
        else
        {
#if DEBUG
            INFO("Enc %d lenth aac data!\n", aaclen);
            write(aac_fd, aacbuf, aaclen);
#endif            
        }

        aaclen = 0;
        mk_free(pkt);
    }

    aac_enc_close();
#if DEBUG
    close(aac_fd);
    close(pcm_fd);
    exit(1);
#endif    

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
    if(aac_init(PA_RATE, PA_CHANNELS) < 0)
    {
        sleep(10);
        ERROR("Cannot init aac encoder!\n");
    }
    sleep(3);
    pulse_init(&pu);

    



    pulse_loop_in();
    return 0;
}
