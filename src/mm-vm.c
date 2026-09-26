#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "mm64.h"

struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;
  if (mm->mmap == NULL) return NULL;
  while (pvma != NULL && (int)pvma->vm_id < vmaid) {
    pvma = pvma->vm_next;
  }
  if (pvma == NULL || (int)pvma->vm_id != vmaid) return NULL;
  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
  __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
  return 0;
}

struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  return newrg;
}

int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  if (vmastart >= vmaend) return -1;
  struct vm_area_struct *vma = caller->mm->mmap;
  if (vma == NULL) return -1;
  struct vm_area_struct *cur_area = get_vma_by_num(caller->mm, vmaid);
  if (cur_area == NULL) return -1;
  while (vma != NULL) {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
      return -1;
    vma = vma->vm_next;
  }
  return 0;
}

int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz) {
  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  if (!vma) return -1;
  addr_t old_end = vma->vm_end;
  addr_t new_end = old_end + inc_sz;
  addr_t start_pgn = (old_end + PAGING64_PAGESZ - 1) / PAGING64_PAGESZ;
  addr_t end_pgn = (new_end - 1) / PAGING64_PAGESZ;
  for (addr_t pgn = start_pgn; pgn <= end_pgn; pgn++) {
    addr_t fpn;
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0) return -1;
    pte_set_fpn(caller, pgn, fpn);
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }
  vma->vm_end = new_end;
  vma->sbrk = new_end;
  return 0;
}
