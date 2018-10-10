#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <pthread.h>

#include "memkit.h"



/**
 * mk_print_handle_info: print handle info, calc current blocks and total blocks.
 * handle:  [IN]
*/
void mk_print_handle_info(struct MemKitHandle *handle)
{
    if(handle == 0)
    {
        ERROR("Wrong mm handle:%p\n", handle);
        return ;
    }

    int count = 0;
    struct list_head *pos, *next;
    //struct MemKitBlock *pBlk = NULL;
    pthread_mutex_lock(&handle->handle_mtx);
    if(list_empty(&handle->handle_blk_list))
    {
        ERROR("This handle have no left mem block!\n");
        //return ;
    }
    else
    {
        list_for_each_safe(pos, next, &handle->handle_blk_list)
        {
            //pBlk = list_entry(pos, struct MemKitBlock, list);
            count++;
        }
    }
    INFO("All block count:%d, current count:%d\n", handle->handle_blocks, count);

    //struct MemPacket *pkt = NULL;

    pos = NULL;
    next = NULL;
    count = 0;
    if(list_empty(&handle->handle_pkt_list))
    {
        ERROR("This handle have no left mem packet!\n");
        goto EXIT;
    }
    else
    {
        list_for_each_safe(pos, next, &handle->handle_pkt_list)
        {
            //pkt = list_entry(pos, struct MemPacket, list);
            count++;
        }
    }
    pthread_mutex_unlock(&handle->handle_mtx);
    INFO("All packet count:%d, current count:%d\n", handle->handle_packets, count);

EXIT:
    return ;
}

/**
 * mk_print_pkt_info; print packet info.
 * pkt: [IN]
*/
int mk_print_pkt_info(struct MemPacket *pkt)
{
    struct list_head *pos, *next;
    int count = 0;

    if(NULL == pkt)
    {
        ERROR("Wrong params pkt:%p!", pkt);
        goto EXIT;
    }
    if(pkt->magic_head != MEM_PACKET_MAGIC)
    {
        ERROR("Wrong magic head for this pkt!\n");
        goto EXIT;
    }

    INFO("Infomation for pkt >>>>>\n")
    INFO("pkt total_size:%d, blk_num:%d, name:%s, blk_size:%d\n", pkt->total_size, pkt->blk_num, pkt->pkt_name, pkt->handle->handle_block_size);
    INFO("Actually infomation is >>>>>\n");
    pthread_mutex_lock(&pkt->handle->handle_mtx);
    list_for_each_prev_safe(pos, next, &pkt->blks_list)
    {
        count++;
    }
    pthread_mutex_unlock(&pkt->handle->handle_mtx);
    INFO("pkt calc block count is : %d, total_size:%d\n", count, pkt->total_size);

    return 0;
EXIT:
    return -1;    
}

/**
 *  mk_handle_init: Init a handle object, and malloc memory according to mm_blocks and mm_block_size.
 *  handle: [IN], should be pre-defined or malloced pointer.
 *  mm_blocks:  [IN], numbers of blocks to be split.
 *  mm_block_size:[IN],  useful mem length for one block.    
 *  return val: 0 - ok.    
*/
int mk_handle_init(struct MemKitHandle *handle, unsigned int mm_blocks, unsigned int mm_block_size)
{
    char *p = NULL;
    int ret = E_MEM_INIT_FAIL;
    int i = 0;
    char *ptr = NULL;
    struct MemKitBlock *pBlk = NULL;
    struct MemKitTail *pBlkTail = NULL;
    int blk_full_size;
    int pkt_full_size;
    struct MemPacket *pkt = NULL;

    if(handle == NULL || ((mm_block_size * mm_blocks) == 0))
    {
        WARN("Wrong parameters, pmm_list:%p, blocks:%d, block size:%d\n", handle, mm_blocks, mm_block_size);
        ret = E_MEM_INIT_FAIL;
        goto EXIT;
    }

    //init blocks and add all blocks to handle_blk_list
    blk_full_size = mm_block_size + sizeof(struct MemKitBlock) + sizeof(struct MemKitTail);
    p = (char *)malloc(mm_blocks * blk_full_size);
    if(NULL == p)
    {
        WARN("Cannot malloc memory for ptr, error %s!\n", strerror(errno));
        ret = E_MEM_INIT_FAIL;
    }

    INIT_LIST_HEAD(&handle->handle_blk_list);

    ptr = p;
    for(i = 0; i < mm_blocks; i++)
    {
        if(ptr > p + blk_full_size * mm_blocks)
        {
            WARN("Some errors occurs while do mem split, ptr overall malloc mem!\n");
            WARN("i:%d,ptr:%p, p:%p, mm_blocks:%d, mm_blocks_size:%d, block:%d\n", i,ptr, p, mm_blocks, mm_block_size, blk_full_size);
            ret = E_MEM_INIT_FAIL;
            goto EXIT;
        }
        pBlk = (struct MemKitBlock *)ptr;
        pBlk->magic_head = MEM_BLOCK_MAGIC_HEAD;
        pBlk->blk_length = mm_block_size;
        pBlk->blk_offset = 0;
        //pBlk->blk_entry  = ptr + sizeof(struct MemKitBlock);

        pBlkTail = (struct MemKitTail *)((char *)pBlk->blk_entry + pBlk->blk_length);
        pBlkTail->magic_tail = MEM_BLOCK_MAGIC_TAIL;
        list_add_tail(&pBlk->list, &handle->handle_blk_list);

        ptr += blk_full_size;
    }
    handle->handle_block_size = mm_block_size;
    handle->handle_blocks = mm_blocks;

    //init packets and add all packets to handle_pkt_list
    pkt_full_size = sizeof(struct MemPacket) + sizeof(struct MemKitTail);
    
    p = (char *)malloc(mm_blocks * pkt_full_size);  //keep packet numbers equals to block numbers.
    if(NULL == p)
    {
        ERROR("Cannot malloc memory for pkt, error:%s\n", strerror(errno));
        ret = E_MEM_INIT_FAIL;
        goto EXIT;
    } 

    INIT_LIST_HEAD(&handle->handle_pkt_list);
    ptr = p;
    for(i = 0; i < mm_blocks; i++)
    {
        if(ptr > p + pkt_full_size * mm_blocks)
        {
            WARN("Some errors occurs while do mem split, ptr overall malloc mem!\n");
            WARN("ptr:%p, p:%p, mm_blocks:%d, mm_blocks_size:%d, block:%d\n", ptr, p, mm_blocks, mm_block_size, blk_full_size);
            ret = E_MEM_INIT_FAIL;
            goto EXIT;
        }
        pkt = (struct MemPacket *)ptr;
        pkt->magic_head = MEM_PACKET_MAGIC;
        pkt->blk_num = 0;
        pkt->handle = handle;
        list_add_tail(&pkt->list, &handle->handle_pkt_list);
        ptr += pkt_full_size;
    }
    handle->handle_packet_size = 0; //This packet do not used to store data.
    handle->handle_packets = mm_blocks; //keep same nums to blocks.

    pthread_mutex_init(&handle->handle_mtx, NULL);

    return E_MEM_OK;

EXIT:
    if(p != NULL)
    {
        free(p);
        p = NULL;
    }
    return ret;
}

int mk_handle_deinit(struct MemKitHandle *handle)
{
    return 0;
}

static int mk_magic_check(struct MemKitBlock *pBlk)
{
    int ret = 0;
    struct MemKitTail *pBlkTail = NULL;

    if(pBlk->magic_head != MEM_BLOCK_MAGIC_HEAD)
    {
        ERROR("Magic head have been damaged origin(0x%X), now(0x%X), idx:%d, blk size:%d\n", MEM_BLOCK_MAGIC_HEAD, pBlk->magic_head, pBlk->blk_idx, pBlk->blk_length);
        ret = -1;
        return ret;
    }
    
    pBlkTail = (struct MemKitTail *)((char *)pBlk->blk_entry + pBlk->blk_length);
    if(pBlkTail->magic_tail != MEM_BLOCK_MAGIC_TAIL)
    {
        ERROR("Magic tail have been damaged origin(0x%X), now(0x%X)\n", MEM_BLOCK_MAGIC_TAIL, pBlkTail->magic_tail);
        ret = -1;
    }

    return ret;
}


/**
 *  mk_malloc: apply pakcet node and blocks. If size cannot be divisibled by mm_block_size, then used one more full block.
 *  handle:[IN], 
 *  size:   [IN], The mem length will be applied.
 *  name:   [IN], set the packet name . If name == NULL, it will be PI-MEM 
 *  return val: NULL - failed. not NULL for ok.
*/
struct MemPacket * mk_malloc(struct MemKitHandle *handle, unsigned int size, char *name)
{
    struct MemPacket *pkt = NULL;
    struct list_head *pos, *next;
    if(NULL == handle || size == 0)
    {
        WARN("Wrong params handle:%p, size:%u!\n", handle, size);
        goto EXIT;
    }

    pthread_mutex_lock(&handle->handle_mtx);
    if(list_empty(&handle->handle_pkt_list))
    {
        WARN("NO free packet in handle packet list!\n");
        pkt = NULL;
        pthread_mutex_unlock(&handle->handle_mtx);
        goto EXIT;
    }
    else
    {
        //INFO("Malloc from handle packe for name:%s\n", name);
        list_for_each_safe(pos, next, &handle->handle_pkt_list)
        {
            pkt = list_entry(pos, struct MemPacket, list);
            if(!pkt)
            {
                ERROR("Wrong with get entry address for pkt!\n");
                exit(1);
            }
            list_del(&pkt->list);
            break;
        }
    }

    pthread_mutex_unlock(&handle->handle_mtx);
    
    if(pkt->magic_head != MEM_PACKET_MAGIC)
    {
        ERROR("Packet %p:%s have been damaged origin:(0x%X), now(0x%X)!\n", pkt, pkt->pkt_name, MEM_PACKET_MAGIC, pkt->magic_head);
        return NULL;
    }

    pkt->mem_refs = 1;
    pkt->handle = handle;
    pkt->total_size = 0;
    pkt->blk_num = 0;
    strncpy(pkt->pkt_name, name == NULL ? "PI-MEM" : name, 7);
    INIT_LIST_HEAD(&pkt->blks_list);

    if(mk_realloc(pkt, size) == -1)
    {
        //need to free pkt and set error return value
        mk_free(pkt);
    }
    //pthread_mutex_unlock(&handle->handle_mtx);

EXIT:
    return pkt;
}

/**
 *  mk_realloc :  get memory block from handle block list.
 *  pkt :   packet pointer.
 *  size   :    the memory size need to applied for.
 *  fail:-1, ok:0.
*/
int mk_realloc(struct MemPacket *pkt ,int size)
{
    //struct MemPacket *packet = NULL;
    struct MemKitBlock *pBlk = NULL;
    struct list_head *pos, *next;
    struct MemKitHandle *handle = NULL;
    int len = 0;
    int diff = 0;

    if(NULL == pkt || size == 0)
    {
        ERROR("Wrong params, pkt:%p, handle:%p!\n", pkt, handle);
        goto EXIT;
    }

    handle = pkt->handle;
    //pkt->total_size += size;
    //if((handle->handle_block_size * pkt->blk_num) >= (pkt->total_size))
    if(size <= (handle->handle_block_size * pkt->blk_num - pkt->total_size))
    {
        INFO("Current blocks have enough memory for pkt!\n");
        pkt->total_size += size;
        return 0;
    }

    if(pkt->magic_head != MEM_PACKET_MAGIC)
    {
        ERROR("Wrong pkt magic:0x%x, handle:%p, func %p!\n", pkt->magic_head, pkt->handle, __builtin_return_address(0));
        goto EXIT;
    }

    /**
     * We need to get blocks for extra size of memory. So we must minus this last block's free memory space.
    */
    len = size;
    diff = handle->handle_block_size * pkt->blk_num - pkt->total_size;
    len -= diff;
    pkt->total_size += diff;
    
    //alloc blocks from handle block list.
    pthread_mutex_lock(&handle->handle_mtx);
    
    if(list_empty(&handle->handle_blk_list))
    {
        ERROR("NO free blocks in handle block list!");
        pthread_mutex_unlock(&handle->handle_mtx);
        goto EXIT;
    }
    //INFO("Malloc from handle block for name:%s\n", pkt->pkt_name);
    list_for_each_safe(pos, next, &handle->handle_blk_list)
    {
        pBlk = list_entry(pos, struct MemKitBlock, list); 
        list_del(&pBlk->list);
        if(-1 == mk_magic_check(pBlk))
        {
            ERROR("Cannot malloc from block list!\n");
            len = 0;
            pthread_mutex_unlock(&handle->handle_mtx);
            goto EXIT;
        }
        list_add_tail(&pBlk->list, &pkt->blks_list);
        //INFO("addr for entry:%p!\n", pBlk->blk_entry);
        pBlk->blk_idx = pkt->blk_num;
        pkt->blk_num++;
        if(len > handle->handle_block_size)
        {
            len -= handle->handle_block_size;
            pkt->total_size += handle->handle_block_size;
        }
        else
        {
            pkt->total_size += len;
            len = 0;
            break;
        }
    }
    pthread_mutex_unlock(&handle->handle_mtx);

    if(len == 0)
    {
        return 0;
    }

EXIT:   
    return -1;    
}

/**
 *  mk_free : recycle pkt mem to handle list.
 *  pkt  : [IN]
 *  return val: 0-ok.
*/
int mk_free(struct MemPacket *pkt)
{
    //int ret = -1;
    struct list_head *pos, *next;
    struct MemKitBlock *pBlk = NULL;
    int blk_num = 0;
    if(NULL == pkt)
    {
        WARN("Wrong params pkt:%p!\n", pkt);
        return -1;
    }
    if(pkt->magic_head != MEM_PACKET_MAGIC)
    {
        ERROR("Wrong magic head for this packet:%p, name:%s, func %p!!!\n", pkt, pkt->pkt_name, __builtin_return_address(0));
        exit(1);
    }

    //INFO("Recycle list pkt total_size:%d, name:%s!\n", pkt->total_size, pkt->pkt_name);
    pthread_mutex_lock(&pkt->handle->handle_mtx);
    //recycle all blocks and add to handle blk list.
    list_for_each_safe(pos, next, &pkt->blks_list)
    {
        pBlk = list_entry(pos, struct MemKitBlock, list);
        if(0 != mk_magic_check(pBlk))
        {
            ERROR("block damaged name:%s, func %p\n", pkt->pkt_name, __builtin_return_address(0));
            sleep(1);
            exit(1);
        }
        blk_num++;
        list_del(&pBlk->list);
        list_add_tail(&pBlk->list, &pkt->handle->handle_blk_list);
    }
    if(blk_num != pkt->blk_num)
    {
        ERROR("Wrong, pkt block num (%d)not equal to calc num(%d) in blk_list!\n", pkt->blk_num, blk_num);
        exit(1);
    }
    //recycle struct MemPacket .
    list_add_tail(&pkt->list, &pkt->handle->handle_pkt_list);
    pthread_mutex_unlock(&pkt->handle->handle_mtx);

    return 0;
}


void mk_set_itor(struct MemPacket *pkt, struct MemItorVec *pItor)
{
    if(NULL == pItor || NULL == pkt)
    {
        ERROR("Wrong params pItor:%p, pkt:%p!\n", pItor, pkt);
        return ;
    }

    memset(pItor, 0, sizeof(struct MemItorVec));
    pItor->plist = &pkt->blks_list; 
    pItor->blk_idx = 0;
    pItor->blk_num = pkt->blk_num;
}


/**
 *  mk_next_entry:   
 *  pItor  :  [IN]  itor inited by  packet , mk_set_itor
 *  blk_len :   [OUT] put out this block lenth(memory length)
 *  return val:  -1 for failed. 0 for ok.
*/
int mk_next_entry(struct MemItorVec *pItor, int *blk_len)
{
    //unsigned char *ptr = NULL;
    struct list_head *plist = NULL;
    struct MemKitBlock *pblk = NULL;

    if(NULL == pItor || NULL == blk_len)
    {
        ERROR("Wrong params pItor:%p, blk_len:%p!\n", pItor, blk_len);
        goto EXIT;
    }
    if(pItor->blk_idx == pItor->blk_num)
    {
        //WARN("No free blocks to be Itor , blk num:%d!\n", pItor->blk_num);
        goto EXIT;
    }
    //
    plist = pItor->plist->next;

    pblk = list_entry(plist, struct MemKitBlock, list);
    if(0 != mk_magic_check(pblk))
    {
        ERROR("block damaged idx:%d, func %p\n", pblk->blk_idx, __builtin_return_address(0));
        sleep(1);
        exit(1);
    }
    //INFO("pblk:%p\n", pblk);
    pItor->entry = pblk->blk_entry;
    pItor->blk_length = pblk->blk_length;
    *blk_len = pblk->blk_length;
    pItor->blk_idx++;
    pItor->poffset = (int *)&pblk->blk_offset;
    pItor->plist = plist;

    return 0;

EXIT:  
    return -1;
}


/**
 * mk_copy_in : copy outline memory buf to pkt.
 * pkt : [IN], memory packet struct.
 * in : [IN], input data pointer.
 * in_size: [IN], input data memory size.
 * 
*/
int mk_copy_in(struct MemPacket *pkt, void *in, int in_size)
{
    int ret = -1;
    int allsize = in_size;
    int blocklen = 0;
    int thislen = 0;
    char *pin = (char *)in;

    if(NULL == pkt || NULL == in || in_size <= 0)
    {
        ERROR("Wrong params  pkt:%p, in:%p, in_size:%d\n", pkt, in, in_size);
        goto EXIT;
    }
    struct MemItorVec itor;
    
    mk_set_itor(pkt, &itor);

    while( (0 == mk_next_entry(&itor, &blocklen)) && allsize > 0)
    {
        thislen = blocklen > allsize ? allsize : blocklen;
        memcpy(itor.entry, pin, thislen);
        *itor.poffset = thislen;
        allsize -= thislen;
        pin += thislen;

    }

    return 0;
EXIT:

    return ret;
}