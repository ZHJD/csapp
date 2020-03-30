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

/* 前一个块的块地址 */
#define PREV_BP(bp) (POINTER_SUB(bp, GET_SIZE(POINTER_SUB(bp, 2 * WORD))))

/* 下一个块的块地址 */
#define NEXT_BP(bp) (POINTER_ADD(FTPR(bp), 2 * WORD))

/* 是否已经分配 */
#define IS_ALLOC(bp) (GET((HDPR(bp))) & 0x1)

/* 由于要8字节对齐，header + foot = 8, 因此一个块最少占据16字节 */
#define MIN_BLOCK_SIZE 16


/* 隐式链表首地址 */
static void *heap_listp;

static void *extend_heap(size_t asize);
static void *merge_free_blocks(void *bp);
static inline size_t page_size();
static void *find_fit(size_t asize);
static void *first_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 分配4个位置，保持8个字节对齐，heap_listp是8个字节对齐 */
    if((heap_listp = mem_sbrk(4 * WORD)) == (void *)-1)
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
    
    /* 扩展堆内存失败则返回-1 */
    if(extend_heap(page_size()) == NULL)
    {
        return -1;
    }

    return 0;
}

/*
 * 获取页面大小
 */
static inline size_t page_size()
{
    return mem_pagesize();
}

/*
 * merge_free_blocks - 合并相邻的空闲块，共四种情况
 */
static void *merge_free_blocks(void *bp)
{
    /* 因为有首部和尾部， 所以不需要担心边界问题 */
    void *prev_bp = PREV_BP(bp);
    void *next_bp = NEXT_BP(bp);

    assert(GET(HDPR(prev_bp)) == GET(FTPR(prev_bp)));
    assert(GET(HDPR(bp)) == GET(FTPR(bp)));

    /* 返回值 */
    void *ret_bp;

    size_t total_size = 0;
    
    /* 前后都已经分配 */
    if(IS_ALLOC(prev_bp) && IS_ALLOC(next_bp))
    {
        return bp;
    }
    /* 前一个未分配，后一个已经分哦 */
    else if(!IS_ALLOC(prev_bp) && IS_ALLOC(next_bp))
    {
        total_size = GET_SIZE(HDPR(prev_bp)) + GET_SIZE(HDPR(bp));
        /* 前一个空闲块的头部 */
        PUT(HDPR(prev_bp), PACK(total_size, 0x0));
        /* 当前空闲块的脚部 */
        PUT(FTPR(bp), PACK(total_size, 0x0));
        ret_bp = prev_bp;
    }
    /* 当前块和后一个块 */
    else if(IS_ALLOC(bp) && !IS_ALLOC(next_bp))
    {
        total_size = GET_SIZE(HDPR(bp)) + GET_SIZE(HDPR(next_bp));
        /* 设置当前块首部 */
        PUT(HDPR(bp), PACK(total_size, 0x0));
        /* 设置下个块的尾部 */
        PUT(FTPR(next_bp), PACK(total_size, 0x0));
        ret_bp = bp;
    }
    /* 前后都是空 */
    else
    {
        total_size = GET_SIZE(HDPR(prev_bp)) + GET_SIZE(HDPR(bp)) + GET_SIZE(HDPR(next_bp));
        /* 前一个空闲块的头部 */
        PUT(HDPR(prev_bp), PACK(total_size, 0x0));
        /* 设置下个块的尾部 */
        PUT(FTPR(next_bp), PACK(total_size, 0x0));
        ret_bp = prev_bp;
    
    }
//    printf("ret_bp 0x%p\n", ret_bp);
    assert(ret_bp != NULL);
    return ret_bp;
}

/*
 * 从头开始遍历隐式链表，寻找第一个合适的空闲块
 */
static void *first_fit(size_t asize)
{
    void *bp = NEXT_BP(heap_listp);

    /* 当空闲块不空时 */
    while(GET(HDPR(bp)) != 0x1)
    {
        if(asize <= GET_SIZE(HDPR(bp)) && !IS_ALLOC(bp))
        {
            return bp;
        }
        bp = POINTER_ADD(bp, GET_SIZE(HDPR(bp)));
    }

    return NULL;
}

/*
 * 寻找一个合适的空闲块
 */
static void *find_fit(size_t asize)
{
    return first_fit(asize);
}

/*
 * extend_heap - 为堆增加asize个字节，asize已经8字节对齐
 */
static void *extend_heap(size_t asize)
{
    /* 申请asize个字节，bp作为新块的首地址,并于旧块合并，构造新的尾部 */
    void *bp;
    if((bp = mem_sbrk(asize)) == (void *)-1)
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

    /* 和前面的空闲块合并 */
    return merge_free_blocks(bp);
    //return bp;
}

/*
 * place - 在空闲块bp处分配asize的空间
 */
static void place(void *bp, size_t asize)
{
    /* 分配后剩余的空间 */
    size_t left_size = GET_SIZE(HDPR(bp)) - asize;
    if(left_size < MIN_BLOCK_SIZE)
    {
        PUT(HDPR(bp), PACK(GET_SIZE(HDPR(bp)), 0x1));
        PUT(FTPR(bp), PACK(GET_SIZE(HDPR(bp)), 0x1));
    }
    else // 分割
    {
        PUT(FTPR(bp), PACK(left_size, 0x0));

        PUT(HDPR(bp), PACK(asize, 0x1));

        /* GET_SIZE 依赖于头部，头部已经设置，这里时新的尾部 */
        PUT(FTPR(bp), PACK(asize, 0x1));

        assert(FTPR(bp) + 2 * WORD == HDPR(NEXT_BP(bp)));

        /* 设置下一个空闲块的头部 */
        PUT(HDPR(NEXT_BP(bp)), PACK(left_size, 0x0));
    }

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
 * mm_malloc - 在堆上分配8字节对齐的大于等于size大小的内存
 */
void *mm_malloc1(size_t size)
{
    int asize = ALIGN(size + DWORD);

    void *bp = NULL;
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* 计算要扩展的内存 */
    size_t extend_size = page_size() > asize ? page_size() : asize;

    if((bp = extend_heap(extend_size)) == NULL)
    {
        return NULL;
    }

    place(bp, asize);

    return bp;
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














