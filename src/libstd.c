/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

#include "common.h"
#include "syscall.h"

// Đảm bảo struct sc_regs được định nghĩa (phòng trường hợp syscall.h không thấy)
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

int libsyscall (struct pcb_t *caller,
             uint32_t syscall_idx,
             arg_t a1,
             arg_t a2,
             arg_t a3)
{
   struct sc_regs regs;

   regs.a1 = a1;
   regs.a2 = a2;
   regs.a3 = a3;

   return _syscall(caller->krnl, caller->pid, syscall_idx, &regs);
}
