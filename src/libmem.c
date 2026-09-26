/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#ifndef KERNEL_VMA_ID
#define KERNEL_VMA_ID 1
#endif

#ifndef USER_VMA_ID
#define USER_VMA_ID 0
#endif

#ifndef KERNEL_BASE
#define KERNEL_BASE 0x00080000UL
#endif

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/* ========== Các hàm quản lý vùng nhớ tự do ========== */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;
    struct vm_rg_struct **pp = &mm->mmap->vm_freerg_list;
    struct vm_rg_struct *cur = *pp;
    while (cur != NULL && cur->rg_start < rg_elmt->rg_start)
    {
        pp = &cur->rg_next;
        cur = cur->rg_next;
    }
    rg_elmt->rg_next = cur;
    *pp = rg_elmt;
    // merge next
    if (rg_elmt->rg_next != NULL && rg_elmt->rg_end == rg_elmt->rg_next->rg_start)
    {
        struct vm_rg_struct *next = rg_elmt->rg_next;
        rg_elmt->rg_end = next->rg_end;
        rg_elmt->rg_next = next->rg_next;
        free(next);
    }
    // merge prev
    struct vm_rg_struct *prev = NULL;
    cur = mm->mmap->vm_freerg_list;
    while (cur != NULL && cur != rg_elmt)
    {
        prev = cur;
        cur = cur->rg_next;
    }
    if (prev != NULL && prev->rg_end == rg_elmt->rg_start)
    {
        prev->rg_end = rg_elmt->rg_end;
        prev->rg_next = rg_elmt->rg_next;
        free(rg_elmt);
    }
    return 0;
}

struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;
    return &mm->symrgtbl[rgid];
}

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    uint32_t pte = pte_get_entry(caller, (addr_t)pgn);
    if (PAGING_PAGE_PRESENT(pte))
    {
        *fpn = (int)PAGING_PTE_FPN(pte);
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
        pte_set_fpn(caller, (addr_t)pgn, frame);
        *fpn = (int)frame;
        return 0;
    }
    return -1;
}

int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
    int pgn = addr / PAGING64_PAGESZ;
    int off = addr % PAGING64_PAGESZ;
    int fpn;
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;
    return MEMPHY_read(caller->krnl->mram, fpn * PAGING64_PAGESZ + off, data);
}

int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
    int pgn = addr / PAGING64_PAGESZ;
    int off = addr % PAGING64_PAGESZ;
    int fpn;
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;
    return MEMPHY_write(caller->krnl->mram, fpn * PAGING64_PAGESZ + off, value);
}

/* ========== Các hàm API cho syscall ========== */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
    printf("liballoc:178\n");
    addr_t addr;
    if (__alloc(proc, 0, reg_index, size, &addr) == -1)
        return -1;
#ifdef IODUMP
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
#endif
    return 0;
}

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
    printf("libfree:218\n");
    if (__free(proc, 0, reg_index) == -1)
        return -1;
#ifdef IODUMP
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
#endif
    return 0;
}

int libread(struct pcb_t *proc, uint32_t source, addr_t offset, uint32_t *destination)
{
    printf("libread:426\n");
    BYTE data;
    if (__read(proc, 0, source, offset, &data) == -1)
        return -1;
    *destination = (uint32_t)(unsigned char)data;
#ifdef IODUMP
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
#endif
    return 0;
}

int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, addr_t offset)
{
    printf("libwrite:502\n");
    if (__write(proc, 0, destination, offset, data) == -1)
        return -1;
#ifdef IODUMP
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
#endif
    return 0;
}

/* ========== Các hàm kmem, cache, copy (giữ nguyên nhưng không in gì thêm) ========== */
int libkmem_malloc(struct pcb_t *caller, uint32_t size, uint32_t reg_index)
{
    return libsyscall(caller, 440, (arg_t)size, (arg_t)reg_index, 0);
}

addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
    return __alloc(caller, KERNEL_VMA_ID, rgid, size, alloc_addr);
}

int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
    return libsyscall(caller, 442, (arg_t)size, (arg_t)align, (arg_t)cache_pool_id);
}

int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
    return libsyscall(proc, 443, (arg_t)cache_pool_id, (arg_t)reg_index, 0);
}

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
    if (!caller || !caller->mm || !caller->mm->kcpooltbl)
        return (addr_t)-1;
    if (cache_pool_id < 0 || cache_pool_id >= PAGING_MAX_SYMTBL_SZ)
        return (addr_t)-1;
    addr_t obj_size = (addr_t)caller->mm->kcpooltbl[cache_pool_id].size;
    if (obj_size == 0)
        return (addr_t)-1;
    return __alloc(caller, KERNEL_VMA_ID, rgid, obj_size, alloc_addr);
}

int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
    (void)offset;
    return libsyscall(caller, 444, (arg_t)source, (arg_t)destination, (arg_t)size);
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
    (void)offset;
    return libsyscall(caller, 445, (arg_t)source, (arg_t)destination, (arg_t)size);
}

int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
    if (!caller || !caller->mm)
        return -1;
    struct mm_struct *mm = caller->mm;
    struct vm_rg_struct *rg = get_symrg_byid(mm, rgid);
    if (!rg || rg->rg_start >= rg->rg_end)
        return -1;
    addr_t addr = rg->rg_start + offset;
    if (addr >= KERNEL_BASE)
        return pg_getval(mm, (int)addr, data, caller);
    return -1;
}

int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
    if (!caller || !caller->mm)
        return -1;
    struct mm_struct *mm = caller->mm;
    struct vm_rg_struct *rg = get_symrg_byid(mm, rgid);
    if (!rg || rg->rg_start >= rg->rg_end)
        return -1;
    addr_t addr = rg->rg_start + offset;
    if (addr >= KERNEL_BASE)
        return pg_setval(mm, (int)addr, value, caller);
    return -1;
}

int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
    if (!caller || !caller->mm)
        return -1;
    struct mm_struct *mm = caller->mm;
    struct vm_rg_struct *rg = get_symrg_byid(mm, rgid);
    if (!rg || rg->rg_start >= rg->rg_end)
        return -1;
    addr_t addr = rg->rg_start + offset;
    if (addr >= KERNEL_BASE)
        return -1;
    return pg_getval(mm, (int)addr, data, caller);
}

int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
    if (!caller || !caller->mm)
        return -1;
    struct mm_struct *mm = caller->mm;
    struct vm_rg_struct *rg = get_symrg_byid(mm, rgid);
    if (!rg || rg->rg_start >= rg->rg_end)
        return -1;
    addr_t addr = rg->rg_start + offset;
    if (addr >= KERNEL_BASE)
        return -1;
    return pg_setval(mm, (int)addr, value, caller);
}

int free_pcb_memph(struct pcb_t *caller)
{
    if (!caller || !caller->mm)
        return -1;
    struct memphy_struct *mram = caller->krnl->mram;
    if (!mram)
        return -1;
    struct framephy_struct **pp = &mram->used_fp_list;
    while (*pp)
    {
        struct framephy_struct *fp = *pp;
        if (fp->owner == caller->mm)
        {
            *pp = fp->fp_next;
            fp->fp_next = mram->free_fp_list;
            mram->free_fp_list = fp;
        }
        else
        {
            pp = &fp->fp_next;
        }
    }
    return 0;
}

int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    if (!cur_vma)
        return -1;
    struct vm_rg_struct **pp = &cur_vma->vm_freerg_list;
    struct vm_rg_struct *rgit = *pp;
    if (rgit == NULL)
        return -1;
    newrg->rg_start = newrg->rg_end = -1;
    while (rgit != NULL)
    {
        if (rgit->rg_start + size <= rgit->rg_end)
        {
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;
            if (rgit->rg_start + size < rgit->rg_end)
            {
                rgit->rg_start = rgit->rg_start + size;
            }
            else
            {
                *pp = rgit->rg_next;
                free(rgit);
            }
            break;
        }
        pp = &rgit->rg_next;
        rgit = rgit->rg_next;
    }
    if (newrg->rg_start == -1)
        return -1;
    return 0;
}
