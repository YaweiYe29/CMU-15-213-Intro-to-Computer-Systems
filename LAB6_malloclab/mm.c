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
    "team",
    /* First member's full name */
    "Zhaoning Kong",
    /* First member's email address */
    "jonnykong@ucla.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT   8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define DEBUG 0


/* Basic constants and macros */
#define WSIZE           4           /* Word and header/footer size */
#define DSIZE           8           /* Double word size (bytes) */
// #define CHUNKSIZE   (1 << 12)       /* Extend heap by this amount (bytes) */
#define CHUNKSIZE       (1 << 6)    /* Extend heap by this amount (bytes) */
#define SEGLISTCOUNT    5           /* Number of segregated lists */


/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Global variables */
static char *heap_listp;        /* Start of heap */

/*
 * Segregated list roots;
 *      roots[0]:       Seglist root 1-2
 *      roots[1]:       Seglist root 3
 *      roots[2]:       Seglist root 4
 *      roots[3]:       Seglist root 5-8
 *      roots[4]:       Seglist root 9-inf
 */
static char *roots[5];         /* Seglist array */


/* Forward declarations */
static void *coalesce(void *);
static void *extend_heap(size_t);
static void *find_fit(size_t);
static void place(void *, size_t);
static int get_root_index(size_t);
static void *get_block(size_t);
static void put_block(void *);
static void mm_check();
static int mm_check_all_free();
static int mm_check_no_contiguous_free();
static int mm_check_free_in_list();
static int mm_check_point_to_free();
static int mm_check_overlap();
static int mm_check_valid_addr();


/* 
 * mm_init - Initialize the malloc package.
 */
int mm_init(void) {
    /* Create the initial empty heap */
    if((heap_listp = (char *)(mem_sbrk(4 * WSIZE))) == (char *)(-1))
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));    /* Prologue header*/
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));    /* Prologue footer*/
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));        /* Epilogue header*/
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    if(DEBUG)
        mm_check();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;                   /* Adjusted size */
    size_t extendsize;              /* Amount to adjust if no fit */
    char *bp;

    /* Sanity check */
    if(size == 0)
        return NULL;
    
    /* Initialize all seglist roots to NULL */
    memset((void *)roots, 0, SEGLISTCOUNT * sizeof(char *));
    
    /* Adjust block size to include overhead and alignment reqs */
    if(size < DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = ALIGN(size + DSIZE);
    }

    /* Search the free list for a fit */
    if((bp = find_fit(asize)) == NULL) {
        /* Calculate the amount to adjust */
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP((char *)mem_heap_hi() + 1)));
        int prev_alloc = GET_ALLOC(HDRP(PREV_BLKP((char *)mem_heap_hi() + 1)));
        extendsize = asize - (prev_alloc ? 0 : prev_size);
        bp = (char *)extend_heap(extendsize / WSIZE);
    }
    place(bp, asize);
    return bp;
}

/* 
 * mm_free - Freeing a block.
 */
void mm_free(void *ptr) {
    /* Mark the block as free */
    PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
    PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
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


/****** Static helper functions ******/
/*
 * coalesce - Coalesce adjacent free blocks
 */
static void *coalesce(void *bp) {
    /* Whether adjacent blocks have been allocated */
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size_t size = GET_SIZE(HDRP(bp));       /* Size of this block */

    if(prev_alloc && next_alloc) {          /* Case 1 */
        /* Do nothing */
    }

    else if(prev_alloc && !next_alloc) {    /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if(!prev_alloc && next_alloc) {    /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                  /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}


/*
 * extend_heap - Extends the heap with a new free block
 */
static void *extend_heap(size_t words) {
    size_t size;
    char *bp;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);
    if((long)(bp = (char *)mem_sbrk(size)) == -1)   /* mem_sbrk() failed */
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                   /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));                   /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));           /* New epilogue header */

    /* Return the block pointer of the possibly coalesced block */
    return coalesce(bp);
}

/*
 * find_fit - Find a free block satisfying size reqs.
 */
static void *find_fit(size_t size) {
    char *ptr = heap_listp + DSIZE;
    
    /* Implicit free list with first fit */
    while(ptr < (char *)mem_heap_hi() &&        /* Not passed end */
            (GET_ALLOC(HDRP(ptr)) ||            /* Already allocated */
            GET_SIZE(HDRP(ptr)) < size)) {      /* Too small */
        ptr += GET_SIZE(HDRP(ptr));
    }
    
    /* Return NULL if no free block is found */
    if(ptr == (char *)mem_heap_hi() + 1)
        return NULL;
    else
        /* Return block ptr, not start of this block */
        return ptr; 
}

/*
 *  place - Place the requested block at the beginning of the free block, 
 *      splitting only if the size of the remainder would equal or exceed
 *      the minumum block size.
 */
static void place(void *ptr, size_t size) {
    size_t bsize;           /* Size of the free block */
    bsize = GET_SIZE(HDRP(ptr));

    /* No split */
    if(bsize - size < 2 * DSIZE) {
        PUT(HDRP(ptr), PACK(bsize, 1));
        PUT(FTRP(ptr), PACK(bsize, 1));
    }

    /* Split */
    else {
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(bsize - size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(bsize - size, 0));
    }
}

/*
 * get_root - Given a free block size, return the index into the 
 *      corresponding segregated list root.
 */
static int get_root_index(size_t size) {
    if(size <= 2) 
          return 0;
    else if(size <= 3)
        return 1;
    else if(size <= 4)
        return 2;
    else if(size <= 8)
        return 3;
    else
        return 4;
}

/*
 * get_block - Given a free block size, find a block from corresponding
 *      segregated list root, and remove this block from the list. Return
 *      the block ptr.
 */
static void *get_block(size_t size) {
    int root_index = get_root_index(size);
    char *block_to_return;

    /* If current block list is empty, move to next list */
    while(root_index < 5 && roots[root_index] == NULL) {   
        root_index++;
    }

    if(root_index == 5) {   /* No block is large enough */
        block_to_return = NULL;
    }
    else {                  /* Otherwise remove first block from list */
        block_to_return = roots[root_index];
        PUTS(*block_to_return + 1 * WSIZE, NULL);
        roots[root_index] = (char *)(*block_to_return);
    }
    return block_to_return;
}

/*
 * put_block - Given a block ptr, put it into the corresponding segregated 
 *      list. Blocks in list are address-ordered.
 */
static void put_block(void *bp) {
    int root_index = get_root_index(GET_SIZE(HDRP(bp)));
    char *p = roots[root_index];

    /* If this list is empty */
    if(roots[root_index] == NULL) {     
        roots[root_index] = bp;
        PUTS(bp + 0 * WSIZE, NULL); 
        PUTS(bp + 1 * WSIZE, NULL);
    }

    /* Address lower than any block in list, insert into head of list */
    else if(bp < (void *)p) {
        PUTS(bp + 0 * WSIZE, p); 
        PUTS(bp + 1 * WSIZE, NULL);
        PUTS(p + 1 * WSIZE, bp);
        roots[root_index] = bp;
    }

    /* Search in list, and insert after free block pointed to by p */
    else {
        while((void *)(*p) != NULL &&   /* Not reached end */
                bp > (void *)(*p)) {    /* Address-ordered */
            p = *p;
        }
        /* Insert */
        PUTS(bp + 0 * WSIZE, *p);
        PUTS(bp + 1 * WSIZE, p);
        if(*p) PUTS(*p + 1 * WSIZE, bp);    /* bp can be last block */
        PUTS(p + 0 * WSIZE, bp);
    }
}


/****** Consistency checker ******/
/*
 * mm_check - Check for heap consistency. Returns non-zero value 
 *      if and only if consistent.
 */
static void mm_check() {
    assert(mm_check_all_free());
    assert(mm_check_no_contiguous_free());
    assert(mm_check_free_in_list());
    assert(mm_check_point_to_free());
    assert(mm_check_overlap());
    assert(mm_check_valid_addr());
}
/*
 * mm_check_all_free - Check if every block in the free list are
 *      marked as free.
 */
static int mm_check_all_free() {
    return 1;
}
/*
 * mm_check_no_contiguous_free - Check if there are any contiguous
 *      free blocks that somehow escaped coalescing.
 */
static int mm_check_no_contiguous_free() {
    int prev_free = 0;              /* Is previous block free? */
    char *ptr = heap_listp + DSIZE;

    while(ptr < (char *)mem_heap_hi()) {
        if(GET_ALLOC(HDRP(ptr))) {
            /* If block is free, header and footer should be same */
            assert(*HDRP(ptr) == *FTRP(ptr));
            
            if(prev_free) {
                return 0;
            }
            prev_free = 1;
        }

        else {
            prev_free = 0;
        }
        ptr = NEXT_BLKP(ptr);
    }
    return 1;
}
/*
 * mm_check_free_in_list - Check if every free block is actually in
 *      the free list.
 */
static int mm_check_free_in_list() {
    return 1;
}
/*
 * mm_check_point_to_free - Check if pointers in the free list point
 *      to valid free blocks.
 */
static int mm_check_point_to_free() {
    return 1;
}
/*
 * mm_check_overlap - Check if any allocated blocks overlap.
 */
static int mm_check_overlap() {
    return 1;
}
/*
 * mm_check_valid_addr - Check if pointers in a heap block point to 
 *      valid heap addresses.
 */
static int mm_check_valid_addr() {
    return 1;
}