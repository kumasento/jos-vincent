#ifndef JOS_KERN_BUDDY_H
#define JOS_KERN_BUDDY_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/memlayout.h>
#include <inc/mmu.h>

#include <kern/pmap.h>

#define MAX_FREE_LIST   3
#define UNIT_PSIZE      PGSIZE // 4KB
#define MAX_PSIZE       (UNIT_PSIZE<<(MAX_FREE_LIST-1)) // depends on free_list

#define BSYS_PIDX(psize) _bsys_pidx(__FILE__, __LINE__, (psize))

extern struct PageInfo *pages;
extern size_t npages;
extern struct PageInfo *bsys_pages[MAX_FREE_LIST];
extern size_t bsys_npages;
extern size_t bsys_max_pidx;

static inline size_t
_bsys_pidx(const char *file, int line, size_t psize)
{
    size_t index = 0;
    if (psize % 2 || psize < UNIT_PSIZE || psize > MAX_PSIZE)
        _panic(file, line, "BSYS_PIDX called with invalid psize %u", psize);
    psize >>= PGSHIFT; // shift to start;
    while (!(psize & 1))
        psize >>= 1, index ++;
    return index;
}

#define BSYS_PSIZE(pidx) _bsys_psize(__FILE__, __LINE__, (pidx))

static inline size_t
_bsys_psize(const char *file, int line, size_t pidx)
{
    size_t psize = PGSIZE;
    if (pidx > MAX_FREE_LIST)
        _panic(file, line, "BSYS_PSIZE called with invalid pidx %u", pidx);
    psize <<= pidx;
    return psize;
}

static inline size_t
bsys_pagenum(size_t pidx)
{
    if (pidx >= bsys_max_pidx)
        panic("bsys_pagenum called with invalid pidx");
    if (pidx == bsys_max_pidx-1)
        return bsys_npages-(bsys_pages[pidx]-pages);
    else
        return bsys_pages[pidx+1]-bsys_pages[pidx];
}
static inline size_t
bsys_page2pa(size_t pidx, struct PageInfo *pp)
{
    if (pp-bsys_pages[pidx] >= bsys_pagenum(pidx))
        panic("bsys_page2pa called with invalid pidx or pp");
    size_t idx = pp-bsys_pages[pidx];
    size_t size = BSYS_PSIZE(pidx);
    return idx * size;
}
static inline struct PageInfo *
bsys_pa2page(size_t pidx, size_t pa)
{
    if (pa & (PGSIZE-1))
        panic("bsys_page2pa called with invalid pa");
    size_t pgsize = BSYS_PSIZE(pidx);
    size_t pgidx = pa / pgsize;
    return bsys_pages[pidx] + pgidx;
}

void bsys_free_list_insert(size_t pidx, struct PageInfo *pp,
        struct PageInfo **free_list);
void bsys_free_list_remove(size_t pidx, struct PageInfo *pp,
        struct PageInfo **free_list);

#endif
