#include "stdafx.h"
#include "async.h"
#include "tinycthread.h"
#include <assert.h>
#include <stdlib.h>
#include "actor.h"
#include <stdio.h>

struct B;

typedef struct
{
    Actor actor;
    int i;
    B* pB;
} A;

void A_Init(A* a)
{
    Actor_Init(&a->actor);
    a->i = 0;
    a->pB = NULL;
}

void A_Destroy(A* a)
{
    Actor_Destroy(&a->actor);
}


struct B
{
    Actor actor;
    int i;
    A* pA;
};

void B_Init(B* b)
{
    Actor_Init(&b->actor);
    b->i = 0;
    b->pA = NULL;
}

void B_Destroy(B* b)
{
    Actor_Destroy(&b->actor);
}

void Pong(Result result, Actor* actor, void* data);

void Ping(Result result, Actor* actorA, void* pv)
{
    if (result == RESULT_OK)
    {
        A* a = (A*)actorA;
        printf("ping %d\n", (int)pv);
        
        /*envia mensagem "pong" para actor B*/
        Actor_Post(&a->pB->actor, &Pong, 0);
    }
}

void Pong(Result result, Actor* actorB, void* data)
{
    B* b = (B*)actorB;
    printf("pong\n");
    /*envia mensagem "ping" para actor A*/
    Actor_Post(&b->pA->actor, &Ping, 0);
}


int _tmain(int argc, _TCHAR* argv[])
{
    AsyncInitialize();
    
    A a;
    B b;
    A_Init(&a);
    B_Init(&b);
    a.pB = &b;
    b.pA = &a;

    //Actor_Post(&a.actor, &Ping, 0);
    Actor_PostAfter(5, &a.actor, &Ping, (void*)5);
    
    Sleep(10000);
    AsyncUninitialize();
    printf("done");

    A_Destroy(&a);
    B_Destroy(&b);

    return 0;
}
