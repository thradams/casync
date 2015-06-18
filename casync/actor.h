#pragma once
#include "tinycthread.h"

enum ACTOR_STATE
{
    ACTOR_STATE_NONE,
    ACTOR_STATE_RUNNING,
    ACTOR_STATE_ONQUEUE
};

typedef void(*actor_callback)(struct actor* actor, void*);
struct actor_closure
{
    actor_callback callback;
    void*    callback_data;
};

struct actor
{
    ACTOR_STATE state;
    mtx_t s_queue_mutex;

    struct actor_closure* current_tasks;
    int tasks_size;
    int taks_max_size;

    void* object;
};


int  actor_init(struct actor* actor);
void actor_destroy(struct actor* actor);

int actor_post(struct actor* actor,
    actor_callback callback,
    void* callback_data);
