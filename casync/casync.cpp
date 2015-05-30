

#include "stdafx.h"
#include "async.h"

void f(int, void*)
{
}

int _tmain(int argc, _TCHAR* argv[])
{
    async_pool_init();
    async_pool_run(&f, 0);
    async_pool_join();

	return 0;
}

