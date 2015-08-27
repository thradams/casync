#include "actor.h"


#include <stdlib.h>
#include "async.h"
#include <assert.h>

#define ACTOR_TASKS_MAX_SIZE 100

Result Actor_Init(Actor* actor)
{
    actor->state = ACTOR_STATE_NONE;
    actor->nTasks = 0;
    actor->pTasks = NULL;
    int r = mtx_init(&actor->mutex, mtx_plain);
    return r == thrd_success ? RESULT_OK : RESULT_FAIL;
}

void Actor_Destroy(Actor* actor)
{
    mtx_lock(&actor->mutex);

    if (actor->pTasks != NULL)
    {
        assert(actor->nTasks > 0);
        for (int i = actor->nTasks - 1; i >= 0; i--)
        {
            actor->pTasks[i].callback(RESULT_CANCELED,
                actor,
                actor->pTasks[i].args);
        }
        free(actor->pTasks);
    }

    mtx_unlock(&actor->mutex);

    mtx_destroy(&actor->mutex);    
}

static int Actor_GetMessages(Actor* actor, ActorTask** pptasks)
{
    *pptasks = NULL; //out

    mtx_lock(&actor->mutex);

    int ntasks = actor->nTasks;

    if (ntasks != 0)
    {
        assert(actor->pTasks != NULL);

        actor->state = ACTOR_STATE_RUNNING;
        *pptasks = actor->pTasks;
        actor->pTasks = NULL;
        actor->nTasks = 0;
    }
    else
    {
        actor->state = ACTOR_STATE_NONE;
    }

    mtx_unlock(&actor->mutex);
    return ntasks;
}


static void Actor_ProcessMessages(Result, void* p)
{
    Actor* pActor = (Actor*)(p);

    for (;;)
    {
        ActorTask* pTasks;
        int ntasks = Actor_GetMessages(pActor, &pTasks);

        if (ntasks == 0)
        {
            assert(pTasks == NULL);
            break;
        }

        for (int i = ntasks - 1; i >= 0; i--)
        {
            pTasks[i].callback(RESULT_OK,
                pActor, 
                pTasks[i].args);
        }

        free(pTasks);
    }
}

inline ActorTask* CreateActorTasks()
{
    return (ActorTask*)malloc(sizeof(ActorTask) * ACTOR_TASKS_MAX_SIZE);
}

void Actor_Post(Actor* actor, ActorCallback callback, void* args)
{
    Result result = RESULT_OK;

    mtx_lock(&actor->mutex);

    if (actor->pTasks == NULL)
    {
        actor->pTasks = CreateActorTasks();

        if (actor->pTasks == NULL)
        {
            result = RESULT_OUT_OF_MEM;            
        }
    }

    if (result == RESULT_OK)
    {
        if (actor->nTasks < ACTOR_TASKS_MAX_SIZE)
        {
            actor->pTasks[actor->nTasks].callback = callback;
            actor->pTasks[actor->nTasks].args = args;
            actor->nTasks++;

            switch (actor->state)
            {
            case ACTOR_STATE_NONE:
                actor->state = ACTOR_STATE_ONQUEUE;
                RunAsync(&Actor_ProcessMessages, actor);
                break;

            case ACTOR_STATE_ONQUEUE:
            case ACTOR_STATE_RUNNING:
                break;

            default:
                assert(false);
            }
        }
        else
        {
            result = RESULT_OVERFLOW;
        }
    }

    mtx_unlock(&actor->mutex);

    if (result != RESULT_OK)
    {
        callback(result, actor, args);
    }
}

struct Data
{
    Actor* actor;
    ActorCallback callback;
    void* arg;
};


static void Later(Result r, void* pv)
{
    Data * data = (Data*)pv;

    if (r == RESULT_OK)
    {        
        Actor_Post(data->actor, data->callback, data->arg);
    }
    free(data);
}


void Actor_PostAfter(int nSec, 
                       Actor* actor,
                       ActorCallback callback, 
                       void* arg)
{
    Data* data = (Data*)malloc(sizeof(Data) * 1);
    if (data != NULL)
    {
        data->actor = actor;
        data->callback = callback;
        data->arg = arg;
        RunAsyncAfter(nSec, &Later, data);
    }
    else
    {
        callback(RESULT_OUT_OF_MEM, actor, arg);
    }
}


