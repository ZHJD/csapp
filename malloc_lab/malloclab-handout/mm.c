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
#include <assert.h>
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


#define HEADP(p) ((char*)(p) - (SIZE_T_SIZE))

#define GET_SIZE(p) ((*(size_t *)p) & (~0x7))

/* 是否已经分配 */
#define GET_ALLOC(p) ((*(size_t *)p) & 0x1)

#define GET_VALUE(p) ((*(size_t*)p))

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HEADP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - 2 * SIZE_T_SIZE)))
#define POINT_ADD_BYTE(p, byte) (((char*)p) + byte)


static size_t page_size;

/* 空闲链表头指针 */
static void *heap_listp;

#define POINTER_LESS(first, second) ((size_t)first < (size_t)second)


static inline void set_free(void *p)
{
    GET_VALUE(p) &= ~0x7;
}

/* 
 * get the foot of this block
 */
static inline void *footp(void *p)
{
    return p + GET_SIZE(HEADP(p)) - 2 * SIZE_T_SIZE;
}

/* 
 * put - put value in memory address p
 */
static inline void put(void *p, size_t value)
{
    (*(size_t*)p) = value;
}

/*
 * 分配一个位
 */
static inline size_t pack(size_t size, size_t alloc)
{
    return size | alloc;
}

static void *merge_free_blocks(void *bp)
{
    void *cur_bp = bp;
    void *prev_bp = PREV_BLKP(cur_bp);
    void *next_bp = NEXT_BLKP(cur_bp);

//printf("cur_bp:0x%x, prev_bp size: %d\n", cur_bp, GET_SIZE(((char*)cur_bp - 2 * SIZE_T_SIZE)));

    void *prev_header = HEADP(prev_bp);
    void *next_header = HEADP(next_bp);

    //printf("headp : 0x%xprev_header 0x%x, prev_size :\n", heap_listp, prev_header, GET_SIZE(prev_header));

    size_t cur_size;
    size_t prev_size;
    size_t next_size;
    size_t total_size;

    /* 前后都已经分配，无需合并 */
    if(GET_ALLOC(prev_header) && GET_ALLOC(next_header))
    {
        return cur_bp;
    }
    else if(!GET_ALLOC(prev_header) && GET_ALLOC(next_header))
    {
        cur_size = GET_SIZE(HEADP(cur_bp));
        prev_size = GET_SIZE(prev_header);
        total_size = cur_size + prev_size;
        put(prev_header, pack(total_size, 0x0));
        put(footp(cur_bp), pack(total_size, 0x0));
        cur_bp = prev_bp;
    }
    else if(GET_ALLOC(prev_header) && !GET_ALLOC(next_header))
    {
        cur_size = GET_SIZE(HEADP(cur_bp));
        next_size = GET_SIZE(next_header);
        total_size = cur_size + next_size;
        put(HEADP(cur_bp), pack(total_size, 0x0));
        put(footp(next_bp), pack(total_size, 0x0));
    }
    else
    {
        cur_size = GET_SIZE(HEADP(cur_bp));
        prev_size = GET_SIZE(prev_header);
        next_size = GET_SIZE(next_header);
        total_size = cur_size + prev_size + next_size;
        put(HEADP(cur_bp), pack(total_size, 0x0));
        put(footp(next_bp), pack(total_size, 0x0));
        cur_bp = prev_bp;
    }
    //printf("cur_size: %d\n", GET_SIZE(HEADP(cur_bp)));
    return cur_bp;
}


static void *extend_heap(size_t size)
{
    size_t asize = ALIGN(size);
    void *bp = mem_sbrk(asize);
    if(bp == (void*)-1)
    {
        return NULL;
    }


    /* 前一个块的尾部替换掉 */
    put(HEADP(bp), pack(asize, 0));
    put(footp(bp), pack(asize, 0));

    assert(footp(bp) == HEADP(NEXT_BLKP(bp)) - SIZE_T_SIZE);
    /* 形成新的尾部 */
    put(HEADP(NEXT_BLKP(bp)), pack(0, 1));

    //printf("next_blkp: 0x%x\n", HEADP(NEXT_BLKP(bp)));
    return merge_free_blocks(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    page_size = mem_pagesize();

    void *p = mem_sbrk(3 * SIZE_T_SIZE);
    if((heap_listp = p) == (void*)-1)
    {
        return -1;
    }
    //put(heap_listp, 0);

    /* 头部 */
    put(POINT_ADD_BYTE(heap_listp, 0 * SIZE_T_SIZE), pack(2 * SIZE_T_SIZE, 1));
    /* 脚部 */
    put(POINT_ADD_BYTE(heap_listp, 1 * SIZE_T_SIZE), pack(2 * SIZE_T_SIZE, 1));
    /* 尾部 */
    put(POINT_ADD_BYTE(heap_listp, 2 * SIZE_T_SIZE), pack(0, 1));

    /* 跳过第一个空快 */
    heap_listp = POINT_ADD_BYTE(heap_listp, 2 * SIZE_T_SIZE);

    if(extend_heap(page_size) == NULL)
    {
        return -1;
    }



//    printf("\tinit info\n");
//   printf("page size %d\n", page_size);
//    printf("heap size %d\n", cur_heap_size);
//    printf("heap lo: 0x%x\n", heap_lo);
//    printf("head hi: 0x%x\n", heap_hi);
//    printf("heap_hi - heap_lo %d\n", (char*)heap_hi - (char*)heap_lo);

   // printf("head list p: 0x%x\n", heap_listp);
    return 0;
}


static void *first_fit(size_t size)
{
    //printf("first_fit\n");
    void *tmp_p = heap_listp;
    //int i = 0;
    while(!(GET_SIZE(tmp_p) == 0 && GET_ALLOC(tmp_p)))
    {
       // printf("tmp_p :0x%x size: %d is_alloc: %d\n", tmp_p, GET_SIZE(tmp_p), GET_ALLOC(tmp_p));
        if(size <= GET_SIZE(tmp_p) && !GET_ALLOC(tmp_p))
        {
            return POINT_ADD_BYTE(tmp_p, SIZE_T_SIZE); 
        }
        tmp_p = POINT_ADD_BYTE(tmp_p, GET_SIZE(tmp_p));
        
        //printf("tmp_p :0x%x size: %d is_alloc: %d\n", tmp_p, GET_SIZE(tmp_p), GET_ALLOC(tmp_p));
        
    }
    return NULL;
}



static void place(void *bp, size_t size)
{
    size_t left_size = GET_SIZE((HEADP(bp))) - size;
    /* 忘记考虑这个导致first_fit出现死循环，因为left_size是0 */
    if(left_size == 0)
    {
        put(HEADP(bp), pack(size, 1));
        put(footp(bp), pack(size, 1));
    }
    else if(size == 0)
    {
        set_free(HEADP(bp));
        set_free(footp(bp));
    }
    else
    {
        put(footp(bp), pack(left_size, 0x0));

        put(HEADP(bp), pack(size, 1));
        put(footp(bp), pack(size, 1));

        put(footp(bp) + SIZE_T_SIZE, pack(left_size, 0));
    }
    //printf("left_size %d:\n", left_size);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */ 
void *mm_malloc(size_t size)
{

    if(size == 0)
    {
        return NULL;
    }
    int asize = ALIGN(size + 2 * SIZE_T_SIZE);
    void *bp;
    if((bp = first_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    
    size_t extendsize = asize > page_size ? asize : page_size;
    if((bp = extend_heap(extendsize)) == NULL)
    {
        return NULL;
    }
    //printf("new bp 0x%x, extendsize: %d alloc size %d\n", bp, extendsize, GET_SIZE(HEADP(bp)));
    place(bp, asize);
    return bp;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc1(size_t size)
{
    //static int i = -1;
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
    if(ptr == NULL)
    {
        return;
    }
    set_free(HEADP(ptr));
    set_free(footp(ptr));
    merge_free_blocks(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc1(void *ptr, size_t size)
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


void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
    {
        return mm_malloc(size);
    }
    else
    {
        void *oldptr = ptr;
        void *ret_ptr = oldptr;
        int asize = ALIGN(size + 2 * SIZE_T_SIZE);
        if(size == 0)
        {
            asize = 0;
        }
        size_t old_size = GET_SIZE(HEADP(oldptr));
        if(asize <= old_size)
        {
            place(ptr, asize);
        }
        else
        {
            void *newptr = mm_malloc(size);
            ret_ptr = newptr;
            if(newptr != NULL)
            {
                memcpy(newptr, oldptr, size);
            }
            mm_free(oldptr);
        }
        return ret_ptr;
    }
}











