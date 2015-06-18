#include "actor.h"


#include <stdlib.h>
#include "async.h"
#include <assert.h>

int actor_init(struct actor* actor)
{
    actor->state = ACTOR_STATE_NONE;
    actor->tasks_size = 0;
    actor->taks_max_size = 100;
    actor->current_tasks = 0;
    int r = mtx_init(&actor->s_queue_mutex, mtx_plain);
    return r;
}

void actor_destroy(struct actor* actor)
{
    mtx_destroy(&actor->s_queue_mutex);
}

static int actor_get_messages(actor* actor, struct actor_closure** current_tasks)
{
    *current_tasks = 0;
    int tasks = 0;
    mtx_lock(&actor->s_queue_mutex);
    tasks = actor->tasks_size;

    if (tasks != 0)
    {
        actor->state = ACTOR_STATE_RUNNING;
        *current_tasks = actor->current_tasks;
        actor->current_tasks = 0;
        actor->tasks_size = 0;
    }
    else
    {
        actor->state = ACTOR_STATE_NONE;
    }

    mtx_unlock(&actor->s_queue_mutex);
    return tasks;
}


static void actor_process_messages(int, void* p)
{
    struct actor* a = (struct actor*)(p);

    for (;;)
    {
        struct actor_closure* current_tasks;
        int tasks = actor_get_messages(a, &current_tasks);

        if (tasks == 0)
        {
            break;
        }

        for (int i = tasks - 1; i >= 0; i--)
        {
            current_tasks[i].callback(a, 0);
        }
    }
}

int actor_post(actor* actor,
    actor_callback callback,
    void* callback_data)
{
    int result = 0;
    mtx_lock(&actor->s_queue_mutex);

    if (actor->current_tasks == 0)
    {
        actor->current_tasks =
            (actor_closure*) malloc(sizeof(actor_closure) * actor->taks_max_size);

        if (actor->current_tasks == 0)
        {
            //out of memor
            result = 1;
        }
    }

    if (result == 0)
    {
        actor->current_tasks[actor->tasks_size].callback = callback;
        actor->current_tasks[actor->tasks_size].callback_data = callback_data;
        actor->tasks_size++;

        switch (actor->state)
        {
        case ACTOR_STATE_NONE:
            actor->state = ACTOR_STATE_ONQUEUE;
            async_pool_run(&actor_process_messages, actor);
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
    }

    mtx_unlock(&actor->s_queue_mutex);
    return result;
}
