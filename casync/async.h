#pragma once


typedef void(*execute_task)(int, void*);


int async_pool_init();

int async_pool_run(execute_task exectask,void* arg);

void async_pool_join();
