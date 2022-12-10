#include "kern/schedule/sched.h"
#include "kern/debug/assert.h"
#include "libs/list.h"
#include "kern/sync/sync.h"
#include "kern/schedule/sched_stride.h"
#include "kern/driver/stdio.h"

static list_entry_t g_timer_list;         // 定时器列表
static struct sched_class *g_sched_class; // 调度器
static struct run_queue *g_rq;            // 运行队列
static struct run_queue __rq;

static inline void
sched_class_enqueue(struct proc_struct *proc)
{
    if (proc != g_idle_proc)
    {
        g_sched_class->enqueue(g_rq, proc);
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc)
{
    g_sched_class->dequeue(g_rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void)
{
    return g_sched_class->pick_next(g_rq);
}

static void
sched_class_proc_tick(struct proc_struct *proc)
{
    if (proc != g_idle_proc)
    {
        g_sched_class->proc_tick(g_rq, proc);
    }
    else
    {
        proc->need_resched = 1;
    }
}

void sched_init(void)
{
    list_init(&g_timer_list);

    g_sched_class = &g_stride_sched_class;

    g_rq = &__rq;
    g_rq->max_time_slice = MAX_TIME_SLICE;
    g_sched_class->init(g_rq);

    cprintf("sched class: %s\n", g_sched_class->name);
}

void wakeup_proc(struct proc_struct *proc)
{
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE)
        {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != g_cur_proc)
            {
                sched_class_enqueue(proc);
            }
        }
        else
        {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

void schedule(void)
{
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        g_cur_proc->need_resched = 0;
        if (g_cur_proc->state == PROC_RUNNABLE)
        {
            sched_class_enqueue(g_cur_proc);
        }
        if ((next = sched_class_pick_next()) != NULL)
        {
            sched_class_dequeue(next);
        }
        if (next == NULL)
        {
            next = g_idle_proc;
        }
        next->runs++;
        if (next != g_cur_proc)
        {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
