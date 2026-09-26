#include "cpu.h"
#include "timer.h"
#include "sched.h"
#include "loader.h"
#include "mm.h"
#ifdef MM64
#include "mm64.h"
#endif

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;
static int num_cpus;
static int done = 0;
static struct krnl_t os;

#ifdef MM_PAGING
static addr_t memramsz;
static addr_t memswpsz[PAGING_MAX_MMSWP];

struct mmpaging_ld_args {
    int vmemsz;
    struct memphy_struct *mram;
    struct memphy_struct **mswp;
    struct memphy_struct *active_mswp;
    int active_mswp_id;
    struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
    char ** path;
    unsigned long * start_time;
#ifdef MLQ_SCHED
    unsigned long * prio;
#endif
} ld_processes;
int num_processes;

struct cpu_args {
    struct timer_id_t * timer_id;
    int id;
};

static void * cpu_routine(void * args) {
    struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
    int id = ((struct cpu_args*)args)->id;
    int time_left = 0;
    struct pcb_t * proc = NULL;
    while (1) {
        if (proc == NULL) {
            proc = get_proc();
            if (proc == NULL) {
                next_slot(timer_id);
                continue;
            }
        } else if (proc->pc >= proc->code->size) {
            printf("\tCPU %d: Processed %2d has finished\n", id, proc->pid);
            free(proc);
            proc = get_proc();
            time_left = 0;
        } else if (time_left == 0) {
            printf("\tCPU %d: Put process %2d to run queue\n", id, proc->pid);
            put_proc(proc);
            proc = get_proc();
        }

        if (proc == NULL && done) {
            printf("\tCPU %d stopped\n", id);
            break;
        } else if (proc == NULL) {
            next_slot(timer_id);
            continue;
        } else if (time_left == 0) {
            printf("\tCPU %d: Dispatched process %2d\n", id, proc->pid);
            time_left = time_slot;
        }

        run(proc);
        time_left--;
        next_slot(timer_id);
    }
    detach_event(timer_id);
    pthread_exit(NULL);
}

static void * ld_routine(void * args) {
#ifdef MM_PAGING
    struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
    struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
    struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
    struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
    struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
    int i = 0;
#ifdef MM64
    os.krnl_pgd = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
    os.krnl_p4d = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
    os.krnl_pud = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
    os.krnl_pmd = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
    os.krnl_pt = malloc(PAGING64_MAX_PGN * sizeof(addr_t));
    for (i = 0; i < PAGING64_MAX_PGN; i++) {
        os.krnl_pgd[i] = (addr_t)&os.krnl_p4d;
        os.krnl_p4d[i] = (addr_t)&os.krnl_pud;
        os.krnl_pud[i] = (addr_t)&os.krnl_pmd;
        os.krnl_pmd[i] = (addr_t)&os.krnl_pt;
        os.krnl_pt[i] = 0;
    }
#else
    os.krnl_pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
#endif
    i=0;
    printf("ld_routine\n");
    while (i < num_processes) {
        printf("DEBUG: about to load '%s'\n", ld_processes.path[i]);
        fflush(stdout);
        struct pcb_t * proc = load(ld_processes.path[i]);
        struct krnl_t * krnl = proc->krnl = &os;    
#ifdef MLQ_SCHED
        proc->prio = ld_processes.prio[i];
#endif
        while (current_time() < ld_processes.start_time[i]) {
            next_slot(timer_id);
        }
#ifdef MM_PAGING
        // Mỗi process có mm_struct riêng
        proc->mm = malloc(sizeof(struct mm_struct));
        if (!proc->mm) {
            printf("Failed to allocate mm for PID %d\n", proc->pid);
            exit(1);
        }
        init_mm(proc->mm, proc);
        krnl->mram = mram;
        krnl->mswp = mswp;
        krnl->active_mswp = active_mswp;
#endif
        printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
            ld_processes.path[i], proc->pid, ld_processes.prio[i]);
        add_proc(proc);
        free(ld_processes.path[i]);
        i++;
        next_slot(timer_id);
    }
    free(ld_processes.path);
    free(ld_processes.start_time);
    done = 1;
    detach_event(timer_id);
    pthread_exit(NULL);
}

static void read_config(const char * path) {
    FILE * file;
    if ((file = fopen(path, "r")) == NULL) {
        printf("Cannot find configure file at %s\n", path);
        exit(1);
    }
    char line[512];
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("ERROR: empty config file\n");
        exit(1);
    }
    if (sscanf(line, "%d %d %d", &time_slot, &num_cpus, &num_processes) != 3) {
        printf("ERROR: invalid first line: %s", line);
        exit(1);
    }

    ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);
    ld_processes.start_time = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
#ifdef MLQ_SCHED
    ld_processes.prio = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
#endif

#ifdef MM_PAGING
    int sit;
#ifdef MM_FIXED_MEMSZ
    memramsz  =  0x100000000;
    memswpsz[0] = 0x1000000;
    for(sit = 1; sit < PAGING_MAX_MMSWP; sit++) memswpsz[sit] = 0;
#else
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("ERROR: missing memory sizes line\n");
        exit(1);
    }
    char *tok = strtok(line, " \t\n");
    if (tok) memramsz = atol(tok);
    for(sit = 0; sit < PAGING_MAX_MMSWP; sit++) {
        tok = strtok(NULL, " \t\n");
        if (tok) memswpsz[sit] = atol(tok);
        else memswpsz[sit] = 0;
    }
#endif
#endif

    int i;
    for (i = 0; i < num_processes; i++) {
        if (fgets(line, sizeof(line), file) == NULL) {
            printf("ERROR: not enough process lines (expected %d)\n", num_processes);
            exit(1);
        }
        char proc[256];
#ifdef MLQ_SCHED
        unsigned long start, prio;
        if (sscanf(line, "%lu %255s %lu", &start, proc, &prio) == 2) {
            prio = 1;
        } else if (sscanf(line, "%lu %255s %lu", &start, proc, &prio) != 3) {
            printf("ERROR: invalid process line %d: %s", i, line);
            exit(1);
        }
        ld_processes.start_time[i] = start;
        ld_processes.prio[i] = prio;
#else
        unsigned long start;
        if (sscanf(line, "%lu %255s", &start, proc) != 2) {
            printf("ERROR: invalid process line %d: %s", i, line);
            exit(1);
        }
        ld_processes.start_time[i] = start;
#endif
        ld_processes.path[i] = (char*)malloc(256);
        snprintf(ld_processes.path[i], 256, "input/proc/%s", proc);
        printf("DEBUG: read proc name = '%s'\n", proc);
    }
    fclose(file);
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Usage: os [path to configure file]\n");
        return 1;
    }
    char path[100];
    path[0] = '\0';
    strcat(path, "input/");
    strcat(path, argv[1]);
    read_config(path);

    pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
    struct cpu_args * args = (struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
    pthread_t ld;
    int i;
    for (i = 0; i < num_cpus; i++) {
        args[i].timer_id = attach_event();
        args[i].id = i;
    }
    struct timer_id_t * ld_event = attach_event();
    start_timer();

#ifdef MM_PAGING
    int rdmflag = 1;
    struct memphy_struct mram;
    struct memphy_struct mswp[PAGING_MAX_MMSWP];
    struct memphy_struct *mswp_ptrs[PAGING_MAX_MMSWP];

    init_memphy(&mram, memramsz, rdmflag);
    for (i = 0; i < PAGING_MAX_MMSWP; i++) {
        init_memphy(&mswp[i], memswpsz[i], rdmflag);
        mswp_ptrs[i] = &mswp[i];
    }

    struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));
    mm_ld_args->timer_id = ld_event;
    mm_ld_args->mram = &mram;
    mm_ld_args->mswp = mswp_ptrs;
    mm_ld_args->active_mswp = &mswp[0];
    mm_ld_args->active_mswp_id = 0;
#endif

    init_scheduler();

#ifdef MM_PAGING
    pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);
#else
    pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
    for (i = 0; i < num_cpus; i++) {
        pthread_create(&cpu[i], NULL, cpu_routine, (void*)&args[i]);
    }

    for (i = 0; i < num_cpus; i++) {
        pthread_join(cpu[i], NULL);
    }
    pthread_join(ld, NULL);
    stop_timer();

    return 0;
}
