#ifndef __DPU_PROCESSOR__
#define __DPU_PROCESSOR__

#include <DpuProxy.h>
#include <Tree.h>


class DpuProcessor {

private:
    /* data */
public:
    DpuProcessor(/* args */);
    void run_coroutine(CoroFunc func, int id, int coro_cnt);
    void coro_worker(CoroYield &yield, RequstGen *gen, int coro_id);
    ~DpuProcessor();
};


uint64_t cache_miss[MAX_APP_THREAD][8];
uint64_t cache_hit[MAX_APP_THREAD][8];
uint64_t latency[MAX_APP_THREAD][LATENCY_WINDOWS];

thread_local CoroCall DpuProcessor::worker[define::kMaxCoro];
thread_local CoroCall DpuProcessor::master;
thread_local GlobalAddress path_stack[define::kMaxCoro]
                                     [define::kMaxLevelOfTree];

thread_local Timer timer;
thread_local std::queue<uint16_t> hot_wait_queue;

DpuProcessor::DpuProcessor(/* args */) {

    using namespace std::placeholders;

    assert(coro_cnt <= define::kMaxCoro);
    for (int i = 0; i < coro_cnt; ++i) {
        auto gen = func(i, dsm, id);
        worker[i] = CoroCall(DpuProcessor::bind(&DpuProcessor::coro_worker, this, _1, gen, i));
    }

    master = CoroCall(std::bind(&DpuProcessor::coro_master, this, _1, coro_cnt));

    master();

}

void DpuProcessor::run_coroutine(CoroFunc func, int id, int coro_cnt) {

}

void DpuProcessor::coro_worker(CoroYield &yield, RequstGen *gen, int coro_id) {

}

DpuProcessor::~DpuProcessor() {

}





#endif