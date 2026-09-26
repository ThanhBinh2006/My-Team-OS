/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include <stdint.h>
#include <stdio.h>
#include "syscall.h"

/* Dự phòng: nếu các biến này không được khai báo trong syscall.h, hãy khai báo ở đây */
#ifndef SYSCALL_H
extern const char* sys_call_table[];
extern const int syscall_table_size;
#endif

int __sys_listsyscall(struct krnl_t *krnl, uint32_t pid, struct sc_regs* reg)
{
   for (int i = 0; i < syscall_table_size; i++)
       printf("%s\n", sys_call_table[i]); 

   return 0;
}
