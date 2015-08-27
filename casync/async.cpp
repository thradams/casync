#include "async.h"


#include "stdafx.h"
#include "async.h"
#include "tinycthread.h"
#include <assert.h>
#include "stdlib.h"

#define POOL_SIZE 2
#define MAX_TASKS 100

struct Task
{
    RunAsyncCallback callback;
    void* arg1;
};

static void Task_Init(struct Task* task,
    RunAsyncCallback exectask,
    void* arg1)
{
    task->arg1 = arg1;    
    task->callback = exectask;
}


struct TaskQueue
{
    Task   tasks[MAX_TASKS];
    size_t count;
    Task*  head;
    Task*  tail;
};

static mtx_t     s_queue_mutex;
static thrd_t    s_threads[POOL_SIZE];
static BOOL      s_stop = FALSE;
static cnd_t     s_condition;
static TaskQueue s_queue;


struct TimerTask
{
    Task task;
    TimerTask* pNext;    
    timespec timePoint;
};

TimerTask * s_pTimerTaskHead = NULL;

TimerTask * TimerTask_Create(struct timespec *ts,
                              RunAsyncCallback callback,
                              void* arg)
{
    TimerTask* pTimerTask = 
        (TimerTask*)malloc(sizeof(TimerTask) * 1);
    if (pTimerTask != NULL)
    {
        pTimerTask->task.callback = callback;
        pTimerTask->task.arg1 = arg;
        pTimerTask->pNext = NULL;
        pTimerTask->timePoint = *ts;
        
    }
    return pTimerTask;
}

void TimerTask_Delete(TimerTask *pTimerTask)
{
    free(pTimerTask);
}

void TimerTask_CancelAll(TimerTask *pHeadTask)
{
    if (pHeadTask != NULL)
    {
        TimerTask *current = pHeadTask;
        while (current->pNext != NULL)
        {

            TimerTask *temp = current;
            current = current->pNext;

            temp->task.callback(RESULT_CANCELED, temp->task.arg1);
            TimerTask_Delete(temp);
        }
    }
}

static void TaskQueue_Init(TaskQueue*q)
{
    q->count = 0;
    q->head = q->tasks;
    q->tail = q->tasks;
}

static Result TaskQueue_Push(TaskQueue* q,
    RunAsyncCallback exectask,
    void* arg1)
{
    Result result = RESULT_FAIL;
    if (q->count < MAX_TASKS)
    {
        Task_Init(q->head, exectask, arg1);

        q->head++;

        if (q->head == (q->tasks + MAX_TASKS))
        {
            q->head = q->tasks;
        }
        q->count++;
        result = RESULT_OK;
    }

    return result;
}

static struct Task* TaskQueue_Pop(TaskQueue*q)
{
    Task* pTask = NULL;
    if (q->count >= 0)
    {
        pTask = q->tail;
        q->tail++;
        if (q->tail == (q->tasks + MAX_TASKS))
        {
            q->tail = q->tasks;
        }
        q->count--;
    }
    return pTask;
}

static void TaskQueue_Clear(TaskQueue* queue)
{
    while (queue->count > 0)
    {
        Task *pTask = TaskQueue_Pop(queue);
        pTask->callback(RESULT_CANCELED, pTask->arg1);
    }
}


int Compare(struct timespec* a, struct timespec* b)
{
    if (a->tv_sec == b->tv_sec)
    {
        if (a->tv_nsec == b->tv_nsec)
        {
            return 0;
        }
        return a->tv_nsec > b->tv_nsec ? 1 : -1;
    }
    
    return a->tv_sec > b->tv_sec ? 1 : -1;    
}

int main_loop(void* pData)
{
    for (;;)
    {
        mtx_lock(&s_queue_mutex);

        for (;;)
        {
            //Se pediu para parar não vai dormir
            if (s_stop)
            {
                break;
            }


            //Se tiver tarefas não vai dormir
            if (s_queue.count > 0)
            {
                break;
            }

            //Se tiver um timer expirado não vai dormir
            if (s_pTimerTaskHead != NULL)
            {
                timespec now;
                int r = timespec_get(&now, TIME_UTC);
                if (Compare(&s_pTimerTaskHead->timePoint, &now) <= 0)
                {
                    break;
                }                
            }


            if (s_pTimerTaskHead == NULL)
            {
                //Não tem tarefa nem timer, então dormir
                cnd_wait(&s_condition, &s_queue_mutex);
            }
            else
            {   
                //Não tem tarefa mas tem um timer
                cnd_timedwait(&s_condition,
                              &s_queue_mutex, 
                              &s_pTimerTaskHead->timePoint);  
            }
        }
        
        
        if (!s_stop)
        {
            TimerTask * pTimerTask = NULL;

            if (s_pTimerTaskHead != NULL)
            {
                timespec now;
                int r = timespec_get(&now, TIME_UTC);

                //já venceu?
                if (Compare(&s_pTimerTaskHead->timePoint, &now) <= 0)
                {
                    pTimerTask = s_pTimerTaskHead;
                    s_pTimerTaskHead = s_pTimerTaskHead->pNext;
                }
            }

            Task* pTask = NULL;
            if (s_queue.count > 0)
            {
                pTask = TaskQueue_Pop(&s_queue);
            }

            //Unlock before callback
            mtx_unlock(&s_queue_mutex);

            if (pTask != NULL)
            {
                pTask->callback(RESULT_OK, pTask->arg1);
            }

            if (pTimerTask != NULL)
            {
                pTimerTask->task.callback(RESULT_OK,
                    pTimerTask->task.arg1);

                TimerTask_Delete(pTimerTask);
            }
        }
        else
        {
            //stopped
            mtx_unlock(&s_queue_mutex);  
            break;
        }

    }
    return 0;
}

Result AsyncInitialize()
{
    TaskQueue_Init(&s_queue);

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
    return r == thrd_success ? RESULT_OK : RESULT_FAIL;
}

void RunAsync(RunAsyncCallback callback, void* arg1)
{
    mtx_lock(&s_queue_mutex);

    Result result = TaskQueue_Push(&s_queue, callback, arg1);

    mtx_unlock(&s_queue_mutex);

    if (result == RESULT_OK)
    {
        cnd_signal(&s_condition);
    }
    else    
    {
        callback(result, arg1);
    }    
}

void AsyncUninitialize()
{
    mtx_lock(&s_queue_mutex);
    s_stop = TRUE;
    mtx_unlock(&s_queue_mutex);

    cnd_broadcast(&s_condition);

    for (size_t i = 0; i < POOL_SIZE; ++i)
    {
        int res;
        int r = thrd_join(s_threads[i], &res);
        assert(r == thrd_success);
    }
    
    TaskQueue_Clear(&s_queue);
    TimerTask_CancelAll(s_pTimerTaskHead);    
}


void SortedInsert(struct TimerTask** ppHeadTask,
                  struct TimerTask* pNewTask)
{
    struct TimerTask* current;
    /* Special case for the head end */
    if (*ppHeadTask == NULL || 
        Compare(&(*ppHeadTask)->timePoint, &pNewTask->timePoint) >= 0)
    {
        pNewTask->pNext = *ppHeadTask;
        *ppHeadTask = pNewTask;
    }
    else
    {
        /* Locate the node before the point of insertion */
        current = *ppHeadTask;
        while (current->pNext != NULL &&
               Compare(&current->timePoint, &pNewTask->timePoint) == -1)
        {
            current = current->pNext;
        }
        pNewTask->pNext = current->pNext;
        current->pNext = pNewTask;
    }
}


void RunAsyncAt(struct timespec *ts,
                  RunAsyncCallback callback,
                  void* arg)
{    
    TimerTask* pTimerTask = TimerTask_Create(ts, callback, arg);
       
    Result result = pTimerTask ? RESULT_OK : RESULT_OUT_OF_MEM;
    
    if (result == RESULT_OK)
    {
        mtx_lock(&s_queue_mutex);
       
        SortedInsert(&s_pTimerTaskHead, pTimerTask);

        mtx_unlock(&s_queue_mutex);
        
        cnd_signal(&s_condition);
    }

    if (result != RESULT_OK)
    {
        callback(result,arg);
    }
}

void RunAsyncAfter(int nSec,
    RunAsyncCallback callback,
    void* arg)
{
    timespec now;
    int r = timespec_get(&now, TIME_UTC);
    now.tv_sec += nSec;
    RunAsyncAt(&now, callback, arg);
}