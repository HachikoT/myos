#include "kern/schedule/sched_stride.h"
#include "kern/process/proc.h"
#include "libs/defs.h"
#include "libs/list.h"
#include "libs/skew_heap.h"

/* You should define the BigStride constant here*/
/* LAB6: YOUR CODE */
#define BIG_STRIDE 0x7FFFFFFF /* ??? */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, run_pool);
     struct proc_struct *q = le2proc(b, run_pool);
     int32_t c = p->stride - q->stride;
     if (c > 0)
          return 1;
     else if (c == 0)
          return 0;
     else
          return -1;
}

static void stride_init(struct run_queue *rq)
{
     rq->run_pool = NULL;
     rq->proc_num = 0;
}

// 添加一个待执行的进程到队列
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
     rq->run_pool = skew_heap_insert(rq->run_pool, &(proc->run_pool), proc_stride_comp_f);
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
     {
          proc->time_slice = rq->max_time_slice;
     }
     proc->rq = rq;
     rq->proc_num++;
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
     rq->run_pool = skew_heap_remove(rq->run_pool, &(proc->run_pool), proc_stride_comp_f);
     rq->proc_num--;
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see proj13.1/kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see proj13.1/libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *stride_pick_next(struct run_queue *rq)
{
     if (rq->run_pool == NULL)
          return NULL;
     struct proc_struct *p = le2proc(rq->run_pool, run_pool);

     if (p->priority == 0)
          p->stride += BIG_STRIDE;
     else
          p->stride += BIG_STRIDE / p->priority;
     return p;
}

/*
 * stride_proc_tick works with the tick event of g_cur_proc process. You
 * should check whether the time slices for g_cur_proc process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for g_cur_proc
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
     /* LAB6: YOUR CODE */
     if (proc->time_slice > 0)
     {
          proc->time_slice--;
     }
     if (proc->time_slice == 0)
     {
          proc->need_resched = 1;
     }
}

struct sched_class g_stride_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};
