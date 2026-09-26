/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "mm64.h"
#include "syscall.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define PAGE_SIZE PAGING64_PAGESZ

#ifndef KERNEL_BASE
#define KERNEL_BASE 0x00080000UL
#endif

#if defined(MM64)

extern int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt);
extern int enlist_pgn_node(struct pgn_t **plist, addr_t pgn);

static void remove_pgn_from_fifo(struct mm_struct *mm, addr_t pgn)
{
    struct pgn_t **pp = &mm->fifo_pgn;
    struct pgn_t *cur;
    while ((cur = *pp) != NULL)
    {
        if (cur->pgn == pgn)
        {
            *pp = cur->pg_next;
            free(cur);
            return;
        }
        pp = &cur->pg_next;
    }
}

int find_victim_page(struct pcb_t *caller, addr_t *retpgn)
{
    struct mm_struct *mm = caller->mm;
    struct pgn_t **pp = &mm->fifo_pgn;
    struct pgn_t *cur;

    struct pgn_t **last_valid_pp = NULL;
    struct pgn_t *last_valid_cur = NULL;

    while ((cur = *pp) != NULL)
    {
        uint32_t pte = pte_get_entry(caller, cur->pgn);
        if (PAGING_PAGE_PRESENT(pte))
        {
            last_valid_pp = pp;
            last_valid_cur = cur;
        }
        pp = &cur->pg_next;
    }

    if (last_valid_cur != NULL)
    {
        *retpgn = last_valid_cur->pgn;
        *last_valid_pp = last_valid_cur->pg_next;
        free(last_valid_cur);
        return 0;
    }

    return -1;
}

static addr_t *get_pte_ptr(struct mm_struct *mm, addr_t pgn, int alloc)
{
    if (!mm->pgd)
    {
        if (!alloc)
            return NULL;
        mm->pgd = (addr_t *)calloc(512, sizeof(addr_t));
        if (!mm->pgd)
            return NULL;
    }
    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
    addr_t *p4d = (addr_t *)mm->pgd[pgd_idx];
    if (!p4d)
    {
        if (!alloc)
            return NULL;
        p4d = (addr_t *)calloc(512, sizeof(addr_t));
        if (!p4d)
            return NULL;
        mm->pgd[pgd_idx] = (addr_t)p4d;
    }
    addr_t *pud = (addr_t *)p4d[p4d_idx];
    if (!pud)
    {
        if (!alloc)
            return NULL;
        pud = (addr_t *)calloc(512, sizeof(addr_t));
        if (!pud)
            return NULL;
        p4d[p4d_idx] = (addr_t)pud;
    }
    addr_t *pmd = (addr_t *)pud[pud_idx];
    if (!pmd)
    {
        if (!alloc)
            return NULL;
        pmd = (addr_t *)calloc(512, sizeof(addr_t));
        if (!pmd)
            return NULL;
        pud[pud_idx] = (addr_t)pmd;
    }
    addr_t *pt = (addr_t *)pmd[pmd_idx];
    if (!pt)
    {
        if (!alloc)
            return NULL;
        pt = (addr_t *)calloc(512, sizeof(addr_t));
        if (!pt)
            return NULL;
        pmd[pmd_idx] = (addr_t)pt;
    }
    return &pt[pt_idx];
}

int init_pte(addr_t *pte, int pre, addr_t fpn, int drt, int swp, int swptyp, addr_t swpoff)
{
    if (pre != 0)
    {
        if (swp == 0)
        {
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
        }
        else
        {
            CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
            SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
            SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
        }
    }
    return 0;
}

int get_pd_from_address(addr_t addr, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
    *pgd = (addr & PAGING64_ADDR_PGD_MASK) >> PAGING64_ADDR_PGD_LOBIT;
    *p4d = (addr & PAGING64_ADDR_P4D_MASK) >> PAGING64_ADDR_P4D_LOBIT;
    *pud = (addr & PAGING64_ADDR_PUD_MASK) >> PAGING64_ADDR_PUD_LOBIT;
    *pmd = (addr & PAGING64_ADDR_PMD_MASK) >> PAGING64_ADDR_PMD_LOBIT;
    *pt = (addr & PAGING64_ADDR_PT_MASK) >> PAGING64_ADDR_PT_LOBIT;
    return 0;
}

int get_pd_from_pagenum(addr_t pgn, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt)
{
    return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT, pgd, p4d, pud, pmd, pt);
}

uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
    addr_t *pte = get_pte_ptr(caller->mm, pgn, 0);
    return pte ? (uint32_t)*pte : 0;
}

int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
    addr_t *pte = get_pte_ptr(caller->mm, pgn, 1);
    if (!pte)
        return -1;
    *pte = (addr_t)pte_val;
    return 0;
}

int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
    addr_t pte = 0;
    init_pte(&pte, 1, fpn, 0, 0, 0, 0);
    return pte_set_entry(caller, pgn, (uint32_t)pte);
}

int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
    addr_t pte = 0;
    init_pte(&pte, 1, 0, 0, 1, swptyp, swpoff);
    return pte_set_entry(caller, pgn, (uint32_t)pte);
}

static int get_physical_frame(struct pcb_t *caller, addr_t pgn, addr_t *fpn)
{
    uint32_t pte = pte_get_entry(caller, pgn);
    if (PAGING_PAGE_PRESENT(pte))
    {
        *fpn = PAGING_PTE_FPN(pte);
        return 0;
    }
    if (pte & PAGING_PTE_SWAPPED_MASK)
    {
        int swptyp = (int)GETVAL(pte, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
        addr_t swpoff = GETVAL(pte, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
        addr_t frame;
        if (MEMPHY_get_freefp(caller->krnl->mram, &frame) != 0)
        {
            addr_t vicpgn;
            if (find_victim_page(caller, &vicpgn) != 0)
                return -1;
            uint32_t vic_pte = pte_get_entry(caller, vicpgn);
            if (!(vic_pte & PAGING_PTE_PRESENT_MASK))
                return -1;
            addr_t vic_fpn = PAGING_PTE_FPN(vic_pte);
            struct memphy_struct *mswp = caller->krnl->active_mswp;
            addr_t swp_fpn;
            if (MEMPHY_get_freefp(mswp, &swp_fpn) != 0)
                return -1;
            __swap_cp_page(caller->krnl->mram, vic_fpn, mswp, swp_fpn);
            pte_set_swap(caller, vicpgn, 0, swp_fpn);
            MEMPHY_put_freefp(caller->krnl->mram, vic_fpn);
            if (MEMPHY_get_freefp(caller->krnl->mram, &frame) != 0)
                return -1;
        }
        struct memphy_struct *src_swp = caller->krnl->mswp[swptyp];
        __swap_cp_page(src_swp, swpoff, caller->krnl->mram, frame);
        pte_set_fpn(caller, pgn, frame);
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
        *fpn = frame;
        return 0;
    }
    return -1;
}

int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
    struct mm_struct *mm = caller->mm;
    struct vm_area_struct *vma = get_vma_by_num(mm, vmaid);
    if (!vma)
        return -1;

    if (caller->krnl->mram != NULL &&
        caller->krnl->mram->free_fp_list == NULL &&
        caller->krnl->mram->used_fp_list == NULL &&
        caller->krnl->mram->maxsz >= PAGING64_PAGESZ)
    {
        if (MEMPHY_format(caller->krnl->mram, PAGING64_PAGESZ) != 0)
            return -1;
    }

    struct vm_rg_struct newrg;
    if (get_free_vmrg_area(caller, vmaid, size, &newrg) == 0)
    {
        mm->symrgtbl[rgid] = newrg;
        *alloc_addr = newrg.rg_start;
        return 0;
    }

    addr_t old_sbrk = vma->sbrk;
    addr_t new_sbrk = old_sbrk + size;
    addr_t start_pgn = (old_sbrk + PAGING64_PAGESZ - 1) / PAGING64_PAGESZ;
    addr_t end_pgn = (new_sbrk - 1) / PAGING64_PAGESZ;

    for (addr_t pgn = start_pgn; pgn <= end_pgn; pgn++)
    {
        uint32_t existing_pte = pte_get_entry(caller, pgn);
        if (PAGING_PAGE_PRESENT(existing_pte))
            continue;

        addr_t fpn;
        if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0)
        {
            addr_t vicpgn;
            if (find_victim_page(caller, &vicpgn) != 0)
                return -1;
            uint32_t vic_pte = pte_get_entry(caller, vicpgn);
            if (!(vic_pte & PAGING_PTE_PRESENT_MASK))
                return -1;
            addr_t vic_fpn = PAGING_PTE_FPN(vic_pte);
            struct memphy_struct *mswp = caller->krnl->active_mswp;
            addr_t swp_fpn;
            if (MEMPHY_get_freefp(mswp, &swp_fpn) != 0)
                return -1;
            __swap_cp_page(caller->krnl->mram, vic_fpn, mswp, swp_fpn);
            pte_set_swap(caller, vicpgn, 0, swp_fpn);
            MEMPHY_put_freefp(caller->krnl->mram, vic_fpn);
            if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0)
                return -1;
        }
        pte_set_fpn(caller, pgn, fpn);
        enlist_pgn_node(&mm->fifo_pgn, pgn);
    }

    vma->vm_end = new_sbrk;
    vma->sbrk = new_sbrk;
    mm->symrgtbl[rgid].rg_start = old_sbrk;
    mm->symrgtbl[rgid].rg_end = new_sbrk;
    *alloc_addr = old_sbrk;
    return 0;
}

int __free(struct pcb_t *caller, int vmaid, int rgid)
{
    struct mm_struct *mm = caller->mm;
    struct vm_rg_struct *rg = &mm->symrgtbl[rgid];
    if (rg->rg_start == 0 && rg->rg_end == 0)
        return -1;

    addr_t start_pgn = rg->rg_start / PAGING64_PAGESZ;
    addr_t end_pgn = (rg->rg_end - 1) / PAGING64_PAGESZ;

    for (addr_t pgn = start_pgn; pgn <= end_pgn; pgn++)
    {
        uint32_t pte = pte_get_entry(caller, pgn);
        if (PAGING_PAGE_PRESENT(pte))
        {
            addr_t fpn = PAGING_PTE_FPN(pte);
            MEMPHY_put_freefp(caller->krnl->mram, fpn);
        }
        pte_set_entry(caller, pgn, 0);
        remove_pgn_from_fifo(mm, pgn);
    }

    struct vm_rg_struct *freerg = malloc(sizeof(struct vm_rg_struct));
    freerg->rg_start = rg->rg_start;
    freerg->rg_end = rg->rg_end;
    freerg->rg_next = NULL;
    enlist_vm_freerg_list(mm, freerg);
    rg->rg_start = rg->rg_end = 0;
    return 0;
}

int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
    struct vm_rg_struct *rg = &caller->mm->symrgtbl[rgid];
    if (rg->rg_start == 0 && rg->rg_end == 0)
        return -1;
    addr_t vaddr = rg->rg_start + offset;
    if (vaddr >= rg->rg_end)
        return -1;
    if (vaddr >= KERNEL_BASE)
    {
        printf("[__read] SECURITY: PID=%u denied access to kernel addr\n", caller->pid);
        return -1;
    }
    addr_t pgn = vaddr / PAGING64_PAGESZ;
    addr_t off = vaddr % PAGING64_PAGESZ;
    addr_t fpn;
    if (get_physical_frame(caller, pgn, &fpn) != 0)
        return -1;
    return MEMPHY_read(caller->krnl->mram, fpn * PAGING64_PAGESZ + off, data);
}

int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
    struct vm_rg_struct *rg = &caller->mm->symrgtbl[rgid];
    if (rg->rg_start == 0 && rg->rg_end == 0)
        return -1;
    addr_t vaddr = rg->rg_start + offset;
    if (vaddr >= rg->rg_end)
        return -1;
    if (vaddr >= KERNEL_BASE)
    {
        printf("[__write] SECURITY: PID=%u denied access to kernel addr\n", caller->pid);
        return -1;
    }
    addr_t pgn = vaddr / PAGING64_PAGESZ;
    addr_t off = vaddr % PAGING64_PAGESZ;
    addr_t fpn;
    if (get_physical_frame(caller, pgn, &fpn) != 0)
        return -1;
    return MEMPHY_write(caller->krnl->mram, fpn * PAGING64_PAGESZ + off, value);
}

int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
    for (int i = 0; i < PAGING64_PAGESZ; i++)
    {
        addr_t addrsrc = srcfpn * PAGING64_PAGESZ + i;
        addr_t addrdst = dstfpn * PAGING64_PAGESZ + i;
        BYTE data;
        MEMPHY_read(mpsrc, addrsrc, &data);
        MEMPHY_write(mpdst, addrdst, data);
    }
    return 0;
}

int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
    mm->pgd = (addr_t *)calloc(512, sizeof(addr_t));
    if (!mm->pgd)
        return -1;
    mm->p4d = mm->pud = mm->pmd = mm->pt = NULL;

    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
    struct vm_area_struct *vma1 = malloc(sizeof(struct vm_area_struct));
    if (!vma0 || !vma1)
        return -1;

    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = 0;
    vma0->sbrk = 0;
    vma0->vm_mm = mm;
    vma0->vm_freerg_list = NULL;
    vma0->vm_next = vma1;

    vma1->vm_id = 1;
    vma1->vm_start = KERNEL_BASE;
    vma1->vm_end = KERNEL_BASE;
    vma1->sbrk = KERNEL_BASE;
    vma1->vm_mm = mm;
    vma1->vm_freerg_list = NULL;
    vma1->vm_next = NULL;

    mm->mmap = vma0;

    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
        mm->symrgtbl[i].rg_start = 0;
        mm->symrgtbl[i].rg_end = 0;
        mm->symrgtbl[i].rg_next = NULL;
    }
    mm->fifo_pgn = NULL;
    mm->kcpooltbl = NULL;
    return 0;
}

int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum)
{
    struct mm_struct *mm = caller->mm;
    for (int i = 0; i < pgnum; i++)
    {
        addr_t vaddr = addr + (addr_t)i * PAGING64_PAGESZ;
        addr_t pgn = vaddr / PAGING64_PAGESZ;
        addr_t *pte = get_pte_ptr(mm, pgn, 1);
        if (!pte)
            return -1;
        *pte = 0;
    }
    return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
    struct mm_struct *mm = caller->mm;

    printf("print_pgtbl:\n");

    addr_t limit = (end == (addr_t)-1) ? start + 1024 * 1024 : end; // dump max 1MB
    for (addr_t vaddr = start; vaddr < limit; vaddr += PAGING64_PAGESZ)
    {
        addr_t pgn = vaddr / PAGING64_PAGESZ;
        addr_t *pte_ptr = get_pte_ptr(mm, pgn, 0);
        if (pte_ptr && *pte_ptr != 0)
        {
            uint32_t pte = (uint32_t)*pte_ptr;
            printf("[vaddr: " FORMAT_ADDR " | pgn: " FORMAT_ADDR "] -> PTE: %08x ",
                   vaddr, pgn, pte);
            if (PAGING_PAGE_PRESENT(pte))
                printf("(ONLINE - FPN: %d)\n", PAGING_PTE_FPN(pte));
            else if (pte & PAGING_PTE_SWAPPED_MASK)
                printf("(SWAPPED - To Disk)\n");
            else
                printf("(UNKNOWN STATE)\n");
        }
    }
    return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
    struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
    rgnode->rg_start = rg_start;
    rgnode->rg_end = rg_end;
    rgnode->rg_next = NULL;
    return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
    rgnode->rg_next = *rglist;
    *rglist = rgnode;
    return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
    struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
    pnode->pgn = pgn;
    pnode->pg_next = *plist;
    *plist = pnode;
    return 0;
}

int print_list_fp(struct framephy_struct *ifp) { return 0; }
int print_list_rg(struct vm_rg_struct *irg) { return 0; }
int print_list_vma(struct vm_area_struct *ivma) { return 0; }
int print_list_pgn(struct pgn_t *ip) { return 0; }

addr_t vmap_page_range(struct pcb_t *caller, addr_t addr, int pgnum, struct framephy_struct *frames, struct vm_rg_struct *ret_rg) { return 0; }
addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst) { return 0; }
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg) { return 0; }

#endif
