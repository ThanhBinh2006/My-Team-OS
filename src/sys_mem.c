/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

/* Fallback definition in case syscall.h didn't fully define struct sc_regs */
#ifndef SYSCALL_H
struct sc_regs
{
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

extern pthread_mutex_t queue_lock;

static struct pcb_t *find_pcb_by_pid(struct krnl_t *krnl, uint32_t pid)
{
    if (!krnl->running_list)
        return NULL;
    for (int i = 0; i < krnl->running_list->size; i++)
    {
        struct pcb_t *p = krnl->running_list->proc[i];
        if (p && p->pid == pid)
            return p;
    }
    return NULL;
}

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    int memop = regs->a1;
    BYTE value;

    pthread_mutex_lock(&queue_lock);
    struct pcb_t *caller = find_pcb_by_pid(krnl, pid);
    pthread_mutex_unlock(&queue_lock);

    if (!caller)
    {
        printf("__sys_memmap: Cannot find caller with PID %u\n", pid);
        return -1;
    }

    switch (memop)
    {
    case SYSMEM_MAP_OP:
        vmap_pgd_memset(caller, regs->a2, regs->a3);
        break;
    case SYSMEM_INC_OP:
        inc_vma_limit(caller, regs->a2, regs->a3);
        break;
    case SYSMEM_SWP_OP:
        __mm_swap_page(caller, regs->a2, regs->a3);
        break;
    case SYSMEM_IO_READ:
        MEMPHY_read(caller->krnl->mram, regs->a2, &value);
        regs->a3 = value;
        break;
    case SYSMEM_IO_WRITE:
        MEMPHY_write(caller->krnl->mram, regs->a2, regs->a3);
        break;
    default:
        printf("Memop code: %d\n", memop);
        break;
    }

    return 0;
}
