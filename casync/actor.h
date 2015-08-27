#pragma once
#include "tinycthread.h"
#include "Result.h"

enum ACTOR_STATE
{
  ACTOR_STATE_NONE,
  ACTOR_STATE_RUNNING,
  ACTOR_STATE_ONQUEUE
};

typedef void(*ActorCallback)(Result result, struct Actor* actor, void*);
struct ActorTask
{
  ActorCallback callback;
  void*    args;
};

struct Actor
{
  ACTOR_STATE state;
  mtx_t mutex;

  ActorTask* pTasks;
  int nTasks;
};


Result  Actor_Init(Actor* actor);
void Actor_Destroy(Actor* actor);
void Actor_Post(Actor* actor, ActorCallback callback, void* args);
void Actor_PostAfter(int nSec,
    Actor* actor,
    ActorCallback callback,
    void* args);
