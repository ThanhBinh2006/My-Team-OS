/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "common.h"
#include "os-cfg.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MM_PAGING
#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif
#endif

int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller);
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller);
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid);

#ifndef KERNEL_BASE
#define KERNEL_BASE  0x00080000UL
#endif
#ifndef USER_VMA_ID
#define USER_VMA_ID 0
#endif
#ifndef KERNEL_VMA_ID
#define KERNEL_VMA_ID 1
#endif

#ifndef SYSCALL_H
struct sc_regs {
        arg_t a1;
        arg_t a2;
        arg_t a3;
        arg_t a4;
        arg_t a5;
        arg_t a6;
        uint32_t orig_ax;
        int32_t flags;
};
#endif

static struct pcb_t *find_pcb_by_pid(struct krnl_t *krnl, uint32_t pid)
{
    if (!krnl) return NULL;
    if (krnl->running_list) {
        for (int i = 0; i < krnl->running_list->size; i++) {
            struct pcb_t *p = krnl->running_list->proc[i];
            if (p && p->pid == pid) return p;
        }
    }
#ifdef MLQ_SCHED
    if (krnl->mlq_ready_queue) {
        for (int prio = 0; prio < MAX_PRIO; prio++) {
            for (int i = 0; i < krnl->mlq_ready_queue[prio].size; i++) {
                struct pcb_t *p = krnl->mlq_ready_queue[prio].proc[i];
                if (p && p->pid == pid) return p;
            }
        }
    }
#endif
    if (krnl->ready_queue) {
        for (int i = 0; i < krnl->ready_queue->size; i++) {
            struct pcb_t *p = krnl->ready_queue->proc[i];
            if (p && p->pid == pid) return p;
        }
    }
    return NULL;
}

int __sys_kmalloc(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t size = (uint32_t)regs->a1;
    uint32_t reg_index = (uint32_t)regs->a2;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    addr_t alloc_addr = 0;
    int ret = __alloc(caller, KERNEL_VMA_ID, (int)reg_index, (addr_t)size, &alloc_addr);
    if (ret != 0) return -1;
    caller->regs[reg_index] = alloc_addr;
    return 0;
#else
    return -1;
#endif
}

int __sys_kfree(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t reg_index = (uint32_t)regs->a1;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    int ret = __free(caller, KERNEL_VMA_ID, (int)reg_index);
    if (ret != 0) return -1;
    return 0;
#else
    return -1;
#endif
}

int __sys_kmem_cache_create(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t obj_size = (uint32_t)regs->a1;
    uint32_t align = (uint32_t)regs->a2;
    uint32_t cache_pool_id = (uint32_t)regs->a3;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    struct mm_struct *mm = caller->mm;
    if (!mm) return -1;
    if (!mm->kcpooltbl) {
        mm->kcpooltbl = (struct kcache_pool_struct *)calloc(
            PAGING_MAX_SYMTBL_SZ, sizeof(struct kcache_pool_struct));
        if (!mm->kcpooltbl) return -1;
    }
    if (cache_pool_id >= PAGING_MAX_SYMTBL_SZ) return -1;
    uint32_t aligned_size = obj_size;
    if (align > 1)
        aligned_size = ((obj_size + align - 1) / align) * align;
    mm->kcpooltbl[cache_pool_id].size = (int)aligned_size;
    mm->kcpooltbl[cache_pool_id].align = (int)align;
    mm->kcpooltbl[cache_pool_id].storage = 0;
    return 0;
#else
    return -1;
#endif
}

int __sys_kmem_cache_alloc(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t cache_pool_id = (uint32_t)regs->a1;
    uint32_t reg_index = (uint32_t)regs->a2;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    struct mm_struct *mm = caller->mm;
    if (!mm || !mm->kcpooltbl) return -1;
    if (cache_pool_id >= PAGING_MAX_SYMTBL_SZ) return -1;
    uint32_t obj_size = (uint32_t)mm->kcpooltbl[cache_pool_id].size;
    if (obj_size == 0) return -1;
    addr_t alloc_addr = 0;
    int ret = __alloc(caller, KERNEL_VMA_ID, (int)reg_index, (addr_t)obj_size, &alloc_addr);
    if (ret != 0) return -1;
    caller->regs[reg_index] = alloc_addr;
    return 0;
#else
    return -1;
#endif
}

int __sys_copy_from_user(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t src_reg = (uint32_t)regs->a1;
    uint32_t dst_reg = (uint32_t)regs->a2;
    uint32_t size = (uint32_t)regs->a3;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    addr_t src_addr = caller->regs[src_reg];
    addr_t dst_addr = caller->regs[dst_reg];
    if (src_addr >= KERNEL_BASE) return -1;
    if (dst_addr < KERNEL_BASE) return -1;
    struct mm_struct *mm = caller->mm;
    for (uint32_t i = 0; i < size; i++) {
        BYTE val = 0;
        if (pg_getval(mm, (int)(src_addr + i), &val, caller) != 0) return -1;
        if (pg_setval(mm, (int)(dst_addr + i), val, caller) != 0) return -1;
    }
    return 0;
#else
    return -1;
#endif
}

int __sys_copy_to_user(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
#ifdef MM_PAGING
    uint32_t src_reg = (uint32_t)regs->a1;
    uint32_t dst_reg = (uint32_t)regs->a2;
    uint32_t size = (uint32_t)regs->a3;
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    if (!caller) return -1;
    addr_t src_addr = caller->regs[src_reg];
    addr_t dst_addr = caller->regs[dst_reg];
    if (src_addr < KERNEL_BASE) return -1;
    if (dst_addr >= KERNEL_BASE) return -1;
    struct mm_struct *mm = caller->mm;
    for (uint32_t i = 0; i < size; i++) {
        BYTE val = 0;
        if (pg_getval(mm, (int)(src_addr + i), &val, caller) != 0) return -1;
        if (pg_setval(mm, (int)(dst_addr + i), val, caller) != 0) return -1;
    }
    return 0;
#else
    return -1;
#endif
}
