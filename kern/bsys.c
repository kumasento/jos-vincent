
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/string.h>

#include <kern/bsys.h>

void
bsys_free_list_insert(size_t pidx, struct PageInfo *pp,
        struct PageInfo **free_list)
{
    cprintf("free page inserting: pidx %u pa %u\n", pidx, bsys_page2pa(pidx, pp));
    struct PageInfo *ptr = *free_list;
    if (pp == NULL || pp->pp_ref)
        panic("bsys_free_list_insert called with invalid pp");
    while (ptr != NULL && ptr->pp_link != NULL)
    {
        size_t pa1, pa2, pa;
        pa  = bsys_page2pa(pidx, pp);
        pa1 = bsys_page2pa(pidx, ptr);
        pa2 = bsys_page2pa(pidx, ptr->pp_link);
        if (pa > pa2 && pa < pa1)
            break;
        ptr = ptr->pp_link;
    }
    pp->pp_link = (ptr == NULL) ? NULL : ptr->pp_link;
    if (ptr != NULL)
        ptr->pp_link = pp;
    else
        *free_list = pp;
}
void
bsys_free_list_remove(size_t pidx, struct PageInfo *pp,
        struct PageInfo **free_list)
{
    cprintf("free page removing: pidx %u pa %u\n", pidx, bsys_page2pa(pidx, pp));
    struct PageInfo *ptr = *free_list;
    if (pp == NULL || !pp->pp_ref)
        panic("bsys_free_list_remove called with invalid pp");

    if (*free_list == NULL)
        return ;
    if (*free_list == pp)
    {
        *free_list = pp->pp_link;
        pp->pp_link = NULL;
        return ;
    }
    while (ptr != NULL && ptr->pp_link != NULL)
    {
        size_t pa, pa_next;
        pa      = bsys_page2pa(pidx, pp);
        pa_next = bsys_page2pa(pidx, ptr->pp_link);
        if (pa == pa_next)
            break;
        ptr = ptr->pp_link;
    }
    if (ptr->pp_link != NULL)
    {
        ptr->pp_link = ptr->pp_link->pp_link;
        pp->pp_link = NULL;
    }
}
