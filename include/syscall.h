#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"

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

extern const char* sys_call_table[];
extern const int syscall_table_size;

#ifndef KERNEL_BASE
#define KERNEL_BASE  0x00080000UL
#endif

#ifndef USER_VMA_ID
#define USER_VMA_ID 0
#endif

#ifndef KERNEL_VMA_ID
#define KERNEL_VMA_ID 1
#endif

int __mm_swap_page(struct pcb_t *, addr_t , addr_t);
int libsyscall(struct pcb_t*, uint32_t, arg_t, arg_t, arg_t);
int _syscall(struct krnl_t*, uint32_t, uint32_t, struct sc_regs*);
int __sys_ni_syscall(struct krnl_t*, struct sc_regs*);
int __sys_kmalloc(struct krnl_t*, uint32_t, struct sc_regs*);
int __sys_kfree(struct krnl_t*, uint32_t, struct sc_regs*);
int __sys_kmem_cache_create(struct krnl_t*, uint32_t, struct sc_regs*);
int __sys_kmem_cache_alloc(struct krnl_t*, uint32_t, struct sc_regs*);
int __sys_copy_from_user(struct krnl_t*, uint32_t, struct sc_regs*);
int __sys_copy_to_user(struct krnl_t*, uint32_t, struct sc_regs*);

#endif
