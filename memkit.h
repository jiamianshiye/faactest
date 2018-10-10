#ifndef MEMKIT_H
#define MEMKIT_H
#ifdef __cplusplus
extern "C"{
#endif

#include <pthread.h>
#include "list.h"

#ifndef INFO
#define INFO(fmt, ...) \
        printf("INFO: (%s): (line:%d) " fmt "\n", __func__, __LINE__, ##__VA_ARGS__);
#endif

#ifndef WARN
#define WARN(fmt, ...) \
        printf("WARN: (%s): (line:%d) " fmt "\n", __func__, __LINE__, ##__VA_ARGS__);
#endif

#ifndef ERROR
#define ERROR(fmt, ...) \
        printf("ERROR: (%s): (line:%d) " fmt "\n", __func__, __LINE__, ##__VA_ARGS__);
#endif

#define         MEM_BLOCK_MAGIC_HEAD        (0xAA5049A0)            //default free
#define         MEM_BLOCK_MAGIC_TAIL        (0xAA6470B0)
#define         MEM_PACKET_MAGIC            (MEM_BLOCK_MAGIC_HEAD)


enum E_MEM_RET_TYPE
{
    E_MEM_OK                = 0,
    E_MEM_INIT_FAIL         = -1,
    E_MEM_INIT_MEM_FAIL     = -2,
    E_MEM_GET_BLK_FAIL      = -3,
};


/**
 * Basic mem block unit. 
*/
struct MemKitBlock
{
    struct list_head    list;
    int                 magic_head; //block status. Default is MEM_BLOCK_MAGIC_HEAD
    unsigned int        blk_length; //total useful memory lenth of this block
    unsigned int        blk_offset; //used lenth of this block
    unsigned int        blk_idx;    //Used for packet list index.
    unsigned char       blk_entry[0];   //memory entry of this block.  |--head--|---memory---|--tail--|
};
struct MemKitTail
{
    int                 magic_tail; //must always be MEM_BLOCK_MAGIC. 
};

/**
 * Handle a  list of memory blocks for one time init. Every mem-blocks will be add to handlelist after init option.
*/
struct MemKitHandle
{
    struct list_head    handle_blk_list;        //hold all blocks of alloced free memory
    int                 handle_blocks;      //total blocks. do not change this value.
    int                 handle_block_size;  //useful mem size of one block
    pthread_mutex_t     handle_mtx;         
    struct list_head    handle_pkt_list;      //hold all packets for alloc
    int                 handle_packets;
    int                 handle_packet_size;
};

/**
 * This packet used to be a head for some blocks. For exp:
 * packet---list---{block1--block2---block3---blockN}.
*/
struct MemPacket
{
    struct list_head    list;
    int                 magic_head;
    int                 total_size;
    int                 blk_num;
    int                 mem_refs;       //refernce  numbers . 引用计数.
    struct list_head    blks_list;
    char                pkt_name[8];    //
    struct MemKitHandle *handle;        //pointer to owner handle.
};

struct MemItorVec
{
    struct list_head    *plist;     
    int                 *poffset;       //public, offset of current block that memory used.
    unsigned char       *entry;         //public, mem entry pointer.
    unsigned int        blk_length;     //should be equal to blk size.
    unsigned int        blk_idx;        //privite, index for blk in packet.  include packet.  blk[0]---blk[1]---blk[2]....blk[N].
    unsigned int        blk_num;        //privite, total num for blks.
};

/**
 * mk_print_handle_info: print handle info, calc current blocks and total blocks.
 * handle:  [IN]
*/
void mk_print_handle_info(struct MemKitHandle *handle);

/**
 * mk_print_pkt_info; print packet info.
 * pkt: [IN]
*/
int mk_print_pkt_info(struct MemPacket *pkt);
//head + memblock + tail
/**
 *  mk_handle_init: Init a handle object, and malloc memory according to mm_blocks and mm_block_size.
 *  handle: [IN], should be pre-defined or malloced pointer.
 *  mm_blocks:  [IN], numbers of blocks to be split.
 *  mm_block_size:[IN],  useful mem length for one block.    
 *  return val: 0 - ok.    
*/
int mk_handle_init(struct MemKitHandle *handle, unsigned int mm_blocks, unsigned int mm_block_size);



//malloc -- split -- add head and tail -- add to list//
/**
 *  mk_malloc: apply pakcet node and blocks. If size cannot be divisibled by mm_block_size, then used one more full block.
 *  handle:[IN], 
 *  size:   [IN], The mem length will be applied.
 *  name:   [IN], set the packet name . If name == NULL, it will be PI-MEM 
 *  return val: NULL - failed. not NULL for ok.
*/
struct MemPacket *mk_malloc(struct MemKitHandle *handle, unsigned int size, char *name);


int mk_realloc(struct MemPacket *pkt ,int size);

/**
 *  mk_free : recycle pkt mem to handle list.
 *  pkt  : [IN]
 *  return val: 0-ok.
*/
int mk_free(struct MemPacket *pkt);


void mk_set_itor(struct MemPacket *pkt, struct MemItorVec *pItor);



/**
 *  mk_next_entry:   
 *  pItor  :  [IN]  itor inited by  packet , mk_set_itor
 *  blk_len :   [OUT] put out this block lenth(memory length)
 *  return val:  -1 for failed. 0 for ok.
*/
int mk_next_entry(struct MemItorVec *pItor, int *blk_len);



/**
 * mk_copy_in : copy outline memory buf to pkt.
 * pkt : [IN], memory packet struct.
 * in : [IN], input data pointer.
 * in_size: [IN], input data memory size.
 * 
*/
int mk_copy_in(struct MemPacket *pkt, void *in, int in_size);
#ifdef __cplusplus
}
#endif
#endif