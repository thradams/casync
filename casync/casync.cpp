#include "stdafx.h"
#include "async.h"
#include "tinycthread.h"
#include <assert.h>
#include <stdlib.h>
#include "actor.h"
#include <stdio.h>

struct A
{
    int i;
};
#define A_INIT { 0 }

struct B
{
    int i;
};
#define B_INIT { 0 }


struct A a = A_INIT;
struct actor actorA;

struct B b = B_INIT;
struct actor actorB;

void pong(struct actor* actor, void* data);

void ping(struct actor* actorA, void* data)
{
    printf("ping\n");

    /*envia mensagem "pong" para actor B*/
    actor_post(&actorB, &pong, 0);
}

void pong(struct actor* actorB, void* data)
{
    printf("pong\n");

    /*envia mensagem "ping" para actor A*/
    actor_post(&actorA, &ping, 0);
}


int _tmain(int argc, _TCHAR* argv[])
{
    async_pool_init();

    actor_init(&actorA);
    actorA.object = &a;

    actor_init(&actorB);
    actorB.object = &b;

    /*envia mensagem "ping" para actor A*/
    actor_post(&actorA, &ping, 0);

    async_pool_join();

    return 0;
}
