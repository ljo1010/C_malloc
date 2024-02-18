#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

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
#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) (x > y ? x : y)
#define PACK(size, alloc) (size | alloc)
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))


char *free_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

//명시적 가용블록에서 사용하는 주소를 입력한다.
#define GET_SUCC(bp)(*(void**)((char*)(bp)+WSIZE))
#define GET_PRED(bp)(*(void**)(bp))

static void splice_free_block(void *bp);
static void add_free_block(void *bp);

int mm_init(void)
{
    /* 원래 있던 코드
    char *heap_listp;
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
    */
   if((free_listp = mem_sbrk(8*WSIZE))==(void*)-1)
    return -1;
    PUT(free_listp, 0);
    PUT(free_listp+(1*WSIZE), PACK(2*WSIZE, 1));
    PUT(free_listp+(2*WSIZE), PACK(2*WSIZE, 1));
    PUT(free_listp+(3*WSIZE), PACK(4*WSIZE, 0));
    PUT(free_listp+(4*WSIZE), NULL);
    PUT(free_listp+(5*WSIZE), NULL);
    PUT(free_listp+(6*WSIZE), PACK(4*WSIZE, 0));
    PUT(free_listp+(7*WSIZE), PACK(0, 1));

    free_listp += (4*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return mm_malloc(size);

    if (size <= 0)
    {
        mm_free(ptr);
        return 0;
    }

    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copySize)
        copySize = size;

    memcpy(newptr, ptr, copySize);

    mm_free(ptr);

    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
/*이전에 있던 콜린스
    if (prev_alloc && next_alloc)
    {
        
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
    */
   if (prev_alloc && next_alloc)
    {
        add_free_block(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        splice_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        splice_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        splice_free_block(PREV_BLKP(bp));
        splice_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_free_block(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    /* 원래 있던 코드
    void *bp = mem_heap_lo() + 2 * WSIZE;
    while (GET_SIZE(HDRP(bp)) > 0)
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;
        bp = NEXT_BLKP(bp);
    }
    return NULL;
    */
   void *bp = free_listp;
   while (bp != NULL)
   {
    if((asize<=GET_SIZE(HDRP(bp))))
        return bp;
    bp = GET_SUCC(bp);
   }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    /*원래 있던 함수
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((csize - asize), 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    */

   // LIFO 순서 할당 정책
   /*
   1. 할당한 블록을 가용 리스트에서 제거
   2. 사용하고 남은 사이즈만큼 다른 가용 블록을 만들고 가용 리스트에 추가 
   */
   splice_free_block(bp);
   size_t csize = GET_SIZE(HDRP(bp));

   if((csize-asize)>=(2*DSIZE)) // 블록 크기의 차이가 16보다 같거나 크면 분할
   {
    PUT(HDRP(bp), PACK(asize, 1)); //현재블록에는 필요한 만큼만 할당
    PUT(FTRP(bp), PACK(asize, 1));

    PUT(HDRP(NEXT_BLKP(bp)), PACK((csize-asize), 0));//남은 크기를 다음 블록에 할당

    PUT(FTRP(NEXT_BLKP(bp)), PACK((csize-asize), 0));
    add_free_block(NEXT_BLKP(bp));
   }
   else
   {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
   }

   //주소순서 정책
   /*
   1. 해당 블록을 전부 쓸때만 가용 리스트에서 해당 블록을 제거
   2. 일부만 사용할때는 사용하는 블록의 새로 만들어지는 사이즈를 남은 크기로 변경해서 가용리스트를 제거하고
   추가하는 과정을 줄인다. 
   */
//   size_t csize = GET_SIZE(HDRP(bp));
//   if((csize-asize)>=(2*DSIZE))
//   {
//     PUT(HDRP(bp), PACK(asize, 1));
//     PUT(FTRP(bp), PACK(asize, 1));

//     bp = NEXT_BLKP(bp);

//     PUT(HDRP(bp), PACK((csize-asize), 0));
//     PUT(FTRP(bp), PACK((csize-asize), 0));

//     GET_SUCC(bp)=GET_SUCC(PREV_BLKP(bp));

//     if(PREV_BLKP(bp)==free_listp)
//     {
//         free_listp = bp;
//     }
//     else
//     {
//         GET_PRED(bp)=GET_PRED(PREV_BLKP(bp));
//         GET_SUCC(GET_PRED(PREV_BLKP(bp))) = bp;
//     }
//     if(GET_SUCC(bp) != NULL)
//         GET_PRED(GET_SUCC(bp)) = bp;
//   }
//   else
//   {
//     splice_free_block(bp);
//     PUT(HDRP(bp), PACK(csize, 1));
//     PUT(FTRP(bp), PACK(csize, 1));
//   }
}

static void splice_free_block(void *bp)
{
    if(bp == free_listp)
    {
        free_listp = GET_SUCC(free_listp);
        return;
    }
    
    GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);

    if(GET_SUCC(bp) != NULL)
        GET_PRED(GET_SUCC(bp))=GET_PRED(bp);
}

// 순서 정책 리스트 추가 함수
static void add_free_block(void *bp)
{
    GET_SUCC(bp) = free_listp;
    if(free_listp != NULL)
        GET_PRED(free_listp)=bp;
    free_listp=bp;
}

// 주소 순서 정책에서의 가용 리스트 추가
// static void add_free_block(void *bp)
// {
//     void *currentp = free_listp;
//     if(currentp == NULL)
//     {
//         free_listp = bp;
//         GET_SUCC(bp) = NULL;
//         return;
//     }

//     while (currentp<bp)
//     {
//         if(GET_SUCC(currentp) == NULL||GET_SUCC(currentp)>bp)
//             break;
//             currentp = GET_SUCC(currentp);
//     }

//     GET_SUCC(bp)=GET_SUCC(currentp);
//     GET_SUCC(currentp) =bp;
//     GET_PRED(bp) = currentp;

//     if(GET_SUCC(bp)!=NULL)
//     {
//         GET_PRED(GET_SUCC(bp))=bp;
//     }
// }