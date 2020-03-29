/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 32位系统下可以正常工作 */
#define WORD  4
#define DWORD 8

/* 指针p的内容设置为value */
#define PUT(p, value) ((*(size_t*)(p)) = value)

/* 指针加上byte个字节 */
#define POINTER_ADD(p, byte) (((char*)(p)) + byte)

/* 指针减法 */
#define POINTER_SUB(p, byte) (((char*)(p)) - byte)

/* 对指针解引用 */
#define GET(p) (*(size_t*)(p))

/* 分配 */
#define PACK(size, alloc) (size | alloc) 

/* 获取该块大小 ~0x7把低3位设置为0，高位设为1 */
#define GET_SIZE(p) (GET(p) & ~0x7)

/* 求首部 header ptr */
#define HDPR(bp) (POINTER_SUB(bp, WORD))

/* foot ptr */
#define FTPR(bp) (POINTER_SUB(POINTER_ADD(bp, GET_SIZE(HDPR(bp))), 2 * WORD))

/* 下一个块的块地址 */
#define NEXT_BP(bp) (POINTER_ADD(FTPR(bp), 2 * WORD))

/* 隐式链表首地址 */
static void *heap_listp;


static void *extend_heap(size_t asize);
static void *merge_free_blocks(void *bp);

/*
 *
 */
static void *merge_free_blocks(void *bp)
{
    return NULL;
}

/*
 * extend_heap - 为堆增加asize个字节，asize已经8字节对齐
 */
static void *extend_heap(size_t asize)
{
    /* 申请asize个字节，bp作为新块的首地址,并于旧块合并，构造新的尾部 */
    void *bp;
    if((bp = mem_sbrk(asize)) != (void*)-1)
    {
        return NULL;
    }

    assert(asize % 8 == 0);

    /* 已经8字节对齐，所以可以使用或运算 */

    /* 设置 header， 0 表示该部分未分配 */
    PUT(HDPR(bp), PACK(asize, 0x0));

    /* 设置 foot */
    PUT(FTPR(bp), PACK(asize, 0x0));

    /* 设置结尾部分 */
    PUT(NEXT_BP(bp), PACK(0, 0x1));


    return NULL;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 分配4个位置，保持8个字节对齐，heap_listp是8个字节对齐 */
    if((heap_listp = mem_sbrk(4 * WORD)) == (void*)-1)
    {
        return -1;
    }
    /* 填充0 为了对齐 */
    PUT(POINTER_ADD(heap_listp, 0 * WORD), 0);

    /* 设置头部 */
    PUT(POINTER_ADD(heap_listp, 1 * WORD), PACK(2 * WORD, 1));
    PUT(POINTER_ADD(heap_listp, 2 * WORD), PACK(2 * WORD, 1));  

    /* 设置尾部 */
    PUT(POINTER_ADD(heap_listp, 3 * WORD), PACK(0, 1));
    
    /* 指向首部的block位置 */
    heap_listp = POINTER_ADD(heap_listp, 2 * WORD);
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
    return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














