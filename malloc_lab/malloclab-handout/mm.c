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
#define  NDEBUG
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
    "personal project",
    /* First member's full name */
    "zhang",
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

#define LESS(first, Second) (((size_t)(first)) < ((size_t)(Second)))


/* 针对空闲链表的next和pre */
#define NEXT_FREEPTR(bp) (POINTER_ADD(bp, 1 * WORD))
#define PREV_FREEPTR(bp) (POINTER_ADD(bp, 0 * WORD))

/* 表示空闲块结束 */
#define END 0x1

/***********************************************************
 * 由于对齐原因，最小块是16个字节，因此可以利用空闲块中的8个字节存放
 * prev 和 next 指针，加快空闲块的查找
 * 头部格式
 *  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 * |       |       |       |       |
 * |header |prev   |next   |footer |
 * |_ _ _ _|_ _ _ _|_ _ _ _|_ _ _ _|
 *
 *  free block 格式
 *  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
 * |      |      |     |               |       |
 * |header|prev  |next |    free       |footer |
 * |_ _ _ | _ _ _|_ _ _|_ _ _ _ _ _ _ _|_ _ _ _|
 * 而非空闲块没有中间的prev和next
 * free_listp 指向头结点的prev处
*************************************************************/


/* 隐式链表首地址 */
static void *heap_listp;

/* 指向第一个空闲块 */
static void *free_listp;

static void *extend_heap(size_t asize);
static void *merge_free_blocks(void *bp);
static inline size_t page_size();
static void *find_fit(size_t asize);
static void *first_fit(size_t asize);
static void place(void *bp, size_t asize);
static void mm_check();
static void insert_free_block_in_head(void *bp);
static void remove_free_block(void *bp);

/*
 * mm_check() 检查堆内存的分配情况，便于调试
 */
static void mm_check()
{
    printf("\n********** mm_check() ***************\n");
    void *bp = heap_listp;

    printf("\n********** all blocks ***************\n");
    while(GET(HDPR(bp)) != 0x01)
    {
        printf("bp address: %p, size %d, alloc %d\n", bp, GET_SIZE(HDPR(bp)), IS_ALLOC(bp));
        if(GET(HDPR(bp)) != GET(FTPR(bp)))
        {
            printf("!!!!! header not equal to foot\n");
            exit(1);
        }
        if(LESS(HDPR(NEXT_BP(bp)), FTPR(bp)))
        {
            printf("!!!!overlap occur\n");
            printf("HDPR(NEXT_BP(bp)) = %p, FTPR(bp) = %p\n", HDPR(NEXT_BP(bp)),
                FTPR(bp));
            exit(1);
        }
        bp = NEXT_BP(bp);
    }
    printf("\n********** free blocks ***************\n");

    void *free_bp = free_listp;
    int i = 0;
    while(free_bp != END && i++ < 5)
    {
        printf("free_bp: %p, size %d\n", free_bp, GET_SIZE(HDPR(free_bp)));
        free_bp = (void*)GET(NEXT_FREEPTR(free_bp));
    }

    printf("********** mm_check() done ***************\n");
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 分配4个位置，保持8个字节对齐，heap_listp是8个字节对齐 */
    if((heap_listp = mem_sbrk(6 * WORD)) == (void *)-1)
    {
        return -1;
    }
    /* 头部的header */
    PUT(POINTER_ADD(heap_listp, 1 * WORD), PACK(4 * WORD, 1));

    /* prev */
    PUT(POINTER_ADD(heap_listp, 2 * WORD), END);

    /* next空闲块 */
    PUT(POINTER_ADD(heap_listp, 3 * WORD), END);  

    /* 头部footer */
    PUT(POINTER_ADD(heap_listp, 4 * WORD), PACK(4 * WORD, 1));

    /* 设置尾部 */
    PUT(POINTER_ADD(heap_listp, 5 * WORD), PACK(0, 1));
    
    /* 指向首部的block位置，也就是prev的地方 */
    heap_listp = POINTER_ADD(heap_listp, 2 * WORD);

    /* 空闲块首部同样指向这个位置 */
    free_listp = heap_listp;

    assert(GET(NEXT_FREEPTR(free_listp)) == END);

    assert(IS_ALLOC(heap_listp));
    //printf("\n preface size %d\n", GET_SIZE(HDPR(heap_listp)));
    //printf("\n preface size %d, heap_listp: 0x%p\n", GET_SIZE(HDPR(heap_listp)), heap_listp);
    assert(HDPR(heap_listp) == POINTER_SUB(heap_listp, WORD));
    assert(GET_SIZE(HDPR(heap_listp)) == 2 * WORD);
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
 * 总是插在空闲链表的头部，使用头插法
 */
static void insert_free_block_in_head(void *bp)
{
    /* 头结点的下一个结点 */
    size_t next_bp = GET(NEXT_FREEPTR(free_listp));

    assert(free_listp == heap_listp);

    /* bp的下一个结点是next_bp */
    PUT(NEXT_FREEPTR(bp), next_bp);
    if(next_bp != END)
    {
        /* 如果不为空则bp的后一个结点的前驱是bp */
        PUT(PREV_FREEPTR(next_bp), (size_t)bp);
    }
    /* bp的前驱是free_listp */
    PUT(PREV_FREEPTR(bp), (size_t)free_listp);

    assert(GET(PREV_FREEPTR(bp)) == free_listp);

    /* free_listp的后继是bp */
    PUT(NEXT_FREEPTR(free_listp), (size_t)bp);

    /* 如果插入成功的话，头结点的后继的前驱是头结点本身 */
    assert(GET(PREV_FREEPTR(GET(NEXT_FREEPTR(free_listp)))
        == (size_t)free_listp));
    //mm_check();
}
/*
 * 从空闲链表中去除空闲块
 */
static void remove_free_block(void *bp)
{
    size_t prev_bp = GET(PREV_FREEPTR(bp));
    size_t next_bp = GET(NEXT_FREEPTR(bp));

    /* 如果是结尾块 */
    if(next_bp == END)
    {
        PUT(NEXT_FREEPTR(prev_bp), END);
    }
    else
    {
        next_bp = GET(NEXT_FREEPTR(bp));
        /* 更改后继的前驱 */
        PUT(PREV_FREEPTR(next_bp), prev_bp);

        /* 更改前驱的后继 */
        PUT(NEXT_FREEPTR(prev_bp), next_bp);
    }
    //mm_check();
}

/*
 * merge_free_blocks - 合并相邻的空闲块，共四种情况
 */
static void *merge_free_blocks(void *bp)
{
    //remove_free_block(bp);

    assert(IS_ALLOC(heap_listp) == 1);
    assert(GET((HDPR(heap_listp))) == GET(FTPR(heap_listp)));
    /* 因为有首部和尾部， 所以不需要担心边界问题 */
    void *prev_bp = PREV_BP(bp);
    void *next_bp = NEXT_BP(bp);
   
    assert(GET(HDPR(prev_bp)) == GET(FTPR(prev_bp)));
    assert(GET(HDPR(bp)) == GET(FTPR(bp)));

    /* 返回值 */
    void *ret_bp = bp;

    size_t total_size = 0;
    
    /* 前后都已经分配 */
    if(IS_ALLOC(prev_bp) && IS_ALLOC(next_bp))
    {
        ;
    }
    /* 前一个未分配，后一个已经分哦 */
    else if(!IS_ALLOC(prev_bp) && IS_ALLOC(next_bp))
    {
        remove_free_block(prev_bp);
        total_size = GET_SIZE(HDPR(prev_bp)) + GET_SIZE(HDPR(bp));
        /* 前一个空闲块的头部 */
        PUT(HDPR(prev_bp), PACK(total_size, 0x0));
        /* 当前空闲块的脚部 */
        PUT(FTPR(bp), PACK(total_size, 0x0));
        ret_bp = prev_bp;
    }
    /* 当前块和后一个块 */
    else if(IS_ALLOC(prev_bp) && !IS_ALLOC(next_bp))
    {
        remove_free_block(next_bp);
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
        remove_free_block(prev_bp);
        remove_free_block(next_bp);
        total_size = GET_SIZE(HDPR(prev_bp)) + GET_SIZE(HDPR(bp)) + GET_SIZE(HDPR(next_bp));
        /* 前一个空闲块的头部 */
        PUT(HDPR(prev_bp), PACK(total_size, 0x0));
        /* 设置下个块的尾部 */
        PUT(FTPR(next_bp), PACK(total_size, 0x0));
        ret_bp = prev_bp;
    
    }

    insert_free_block_in_head(ret_bp);
    //printf("ret_bp 0x%p\n", ret_bp);
    assert(ret_bp != NULL);
    return ret_bp;
}

/*
 * 从头开始遍历隐式链表，寻找第一个合适的空闲块
 */
static void *first_fit(size_t asize)
{
    //printf("\n preface size %d, heap_listp: 0x%p\n", GET_SIZE(HDPR(heap_listp)), heap_listp);
    assert(GET_SIZE(HDPR(heap_listp)) == 2 * WORD);
    void *free_bp = (void*)GET(NEXT_FREEPTR(free_listp));
    
    //int i = 0;
    /* 当空闲块不空时 */
    while(free_bp != END)
    {
        if(asize <= GET_SIZE(HDPR(free_bp)) && !IS_ALLOC(free_bp))
        {
            remove_free_block(free_bp);
            return free_bp;
        }
        //printf("bp address 0x%p, bp size: %d alloc: %d, apply size %d\n", bp, GET_SIZE(HDPR(bp)), IS_ALLOC(bp), asize);
        free_bp = (void*)GET(NEXT_FREEPTR(free_bp));
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
    PUT(HDPR(NEXT_BP(bp)), PACK(0, 0x1));
    
    //insert_free_block_in_head(bp);

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

        assert(FTPR(bp) + 1 * WORD == HDPR(NEXT_BP(bp)));

        /* 设置下一个空闲块的头部 */
        PUT(HDPR(NEXT_BP(bp)), PACK(left_size, 0x0));

        assert(GET(HDPR(bp)) == GET(FTPR(bp)));
        assert(GET(HDPR(NEXT_BP(bp))) == GET(FTPR(NEXT_BP(bp))));

        /* 加入剩下的的块 */
        insert_free_block_in_head(NEXT_BP(bp));
    }

    //mm_check();

}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc1(size_t size)
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
void *mm_malloc(size_t size)
{
    int asize = ALIGN(size + DWORD);

    void *bp = NULL;
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        //mm_check();
        return bp;
    }

    /* 计算要扩展的内存 */
    size_t extend_size = page_size() > asize ? page_size() : asize;

    if((bp = extend_heap(extend_size)) == NULL)
    {
        return NULL;
    }
    remove_free_block(bp);
    place(bp, asize);
    //mm_check();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if(ptr == NULL)
    {
        return;
    }
    size_t size = GET_SIZE(HDPR(ptr));
    //printf("free bp 0x%p, size %d\n", ptr, size);
    PUT(HDPR(ptr), PACK(size, 0x0));
    PUT(FTPR(ptr), PACK(size, 0x0));
    //insert_free_block_in_head(ptr);
    merge_free_blocks(ptr);
    //mm_check();
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
    {
        return mm_malloc(size);
    }
    else
    {
        if(size == 0)
        {
            mm_free(ptr);
            return NULL;
        }
        else
        {
            size_t old_size = GET_SIZE(HDPR(ptr));
            size_t asize = ALIGN(size + DWORD);
            void *next_bp = NEXT_BP(ptr);
            if(asize <= old_size)
            {
                //insert_free_block_in_head(ptr);
                place(ptr, asize);
                return ptr;
            }
            else if(!IS_ALLOC(next_bp) && asize <= old_size + GET_SIZE(HDPR(next_bp)))
            {
                /* 去除待合并的后面的空闲块 */
                remove_free_block(next_bp);
                size_t total_size = old_size + GET_SIZE(HDPR(next_bp));
                /* 合并当前块和下一个块 */
                PUT(HDPR(ptr), PACK(total_size, 0x0));
                PUT(FTPR(next_bp), PACK(total_size, 0x0));
                
                //insert_free_block_in_head(ptr);
                place(ptr, asize);
                return ptr;
            }
            else
            {
                void *new_ptr = mm_malloc(asize);
                /* 因为块大小包含了首部和尾部 */
                memcpy(new_ptr, ptr, old_size - DWORD);
                mm_free(ptr);
                return new_ptr;
            }
        }
    }
}
















