#pragma once

#include "Result.h"

typedef void(*RunAsyncCallback)(Result, void*);

Result AsyncInitialize();
void AsyncUninitialize();

void RunAsync(RunAsyncCallback callback,
              void* arg);

void RunAsyncAfter(int nSec,
                   RunAsyncCallback callback,
                   void* arg);
