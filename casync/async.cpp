#include "async.h"


#include "stdafx.h"
#include "async.h"
#include "tinycthread.h"
#include <assert.h>

#define POOL_SIZE 2
#define MAX_TASKS 100

struct task
{
    execute_task exectask;
    void*        arg;
};

static void task_init(struct task* task,
    execute_task exectask,
    void* arg)
{
    task->arg = arg;
    task->exectask = exectask;
}



struct task_queue
{
    struct task  buffer[MAX_TASKS];
    size_t       count;
    struct task* head;
    struct task* tail;
};

static mtx_t             s_queue_mutex;
static thrd_t            s_threads[POOL_SIZE];
static BOOL              s_stop = FALSE;
static cnd_t             s_condition;
static struct task_queue s_queue;

static void task_queue_init(struct task_queue *q)
{
    q->count = 0;
    q->head = q->buffer;
    q->tail = q->buffer;
}

static int task_queue_push(struct task_queue * q,
    execute_task exectask,
    void* arg)
{
    int result = 1;
    if (q->count < MAX_TASKS)
    {
        task_init(q->head,
            exectask,
            arg);

        q->head++;

        if (q->head == (q->buffer + MAX_TASKS))
        {
            q->head = q->buffer;
        }
        q->count++;
        result = 0;
    }

    return 1;
}

static struct task* task_queue_pop(struct task_queue *q)
{
    struct task* task = NULL;
    if (q->count >= 0)
    {
        task = q->tail;
        q->tail++;
        if (q->tail == (q->buffer + MAX_TASKS))
        {
            q->tail = q->buffer;
        }
        q->count--;
    }
    return task;
}

static void task_queue_clear(struct task_queue *q)
{
    while (s_queue.count > 0)
    {
        struct task *p = task_queue_pop(q);
        p->exectask(0, p->arg);
    }
}

int main_loop(void* pData)
{
    for (;;)
    {
        mtx_lock(&s_queue_mutex);

        while (!s_stop &&
            s_queue.count == 0)
        {
            cnd_wait(&s_condition, &s_queue_mutex);
        }

        if (s_stop &&
            s_queue.count == 0)
        {
            mtx_unlock(&s_queue_mutex);
            break;
        }
        else
        {
            struct task *p = task_queue_pop(&s_queue);
            mtx_unlock(&s_queue_mutex);
            (*p->exectask)(1, p->arg);          
        }
    }
    return 0;
}

int async_pool_init()
{
    task_queue_init(&s_queue);

    int r = mtx_init(&s_queue_mutex, mtx_plain);
    if (r == thrd_success)
    {
        r = cnd_init(&s_condition);
        if (r == thrd_success)
        {
            for (int i = 0; i < POOL_SIZE; i++)
            {
                r = thrd_create(&s_threads[i], &main_loop, 0);
                if (r != thrd_success)
                {
                    break;
                }
            }
        }
    }
    return r == thrd_success ? 0 : 1;
}

int async_pool_run(execute_task exectask,
    void* arg)
{
    int result = 0;
    mtx_lock(&s_queue_mutex);

    result = task_queue_push(&s_queue, exectask, arg);

    mtx_unlock(&s_queue_mutex);

    if (result == 0)
    {
        cnd_broadcast(&s_condition);
    }

    return result;
}

void async_pool_join()
{
    BOOL wasstoped = FALSE;
    mtx_lock(&s_queue_mutex);
    wasstoped = s_stop;
    s_stop = TRUE;
    mtx_unlock(&s_queue_mutex);

    if (wasstoped)
    {
        return;
    }

    cnd_broadcast(&s_condition);

    for (size_t i = 0; i < POOL_SIZE; ++i)
    {
        int res;
        int r = thrd_join(s_threads[i], &res);
        assert(r == thrd_success);
    }

    mtx_lock(&s_queue_mutex);
    task_queue_clear(&s_queue);
    mtx_unlock(&s_queue_mutex);
}

