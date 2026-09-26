#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        if(q->size < MAX_QUEUE_SIZE){
                q->proc[q->size] = proc;
                q->size++;
        }
}

struct pcb_t *dequeue(struct queue_t *q)
{
        if(q->size == 0 || empty(q)){
                return NULL;
        }

        struct pcb_t * proc = q->proc[0];
        for(int i = 0 ; i< q->size- 1; i++){
                q->proc[i] = q->proc[i+1]; 
        }

        q->proc[q->size -1] = NULL;
        q->size--;

        return proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        if (q->size == 0 || empty(q)){
                return NULL;
        }

        int target_index = -1;
        
        for(int i = 0 ; i < q->size; i++){
                if(q->proc[i] == proc){
                        target_index = i; 
                        break;
                }
        }

        if(target_index != -1){
                struct pcb_t * target_proc = q->proc[target_index];

                for(int i = target_index; i < q->size-1; i++){
                        q->proc[i] = q->proc[i+1];
                }

                q->proc[q->size - 1] = NULL;
                q->size--;
                
                return target_proc;
        }
        return NULL;
}
