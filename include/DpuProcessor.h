#ifndef __DPU_PROCESSOR__
#define __DPU_PROCESSOR__

#include <queue>
#include "PageStructure.h"
#include "Timer.h"
#include "Tree.h"
#include "DpuProxy.h"

class DpuProcessor {

private:
    DpuProxy *dpuProxy;
    GlobalAddress root_ptr_ptr; 

    static thread_local CoroCall worker[define::kMaxCoro];
    static thread_local CoroCall master;
    static thread_local std::queue<DpuRequest*> task_queue;
public:
    DpuProcessor(DpuProxy *dpuProxy);
    void run_coroutine(int id, int coro_cnt);
    void coro_worker(CoroYield &yield, int coro_id);
    void coro_master(CoroYield &yield, int coro_cnt);


    void run_coroutine_cache_test(int id, int coro_cnt);
    void coro_worker_cache_test(CoroYield &yield, int coro_id);
    void coro_master_cache_test(CoroYield &yield, int coro_cnt);

    ~DpuProcessor();
};





#endif