#ifndef __DPU_PROCESSOR__
#define __DPU_PROCESSOR__

#include <DpuProxy.h>
#include <Tree.h>


class DpuProcesser {

private:
    /* data */
public:
    DpuProcesser(/* args */);
    void run_coroutine(CoroFunc func, int id, int coro_cnt);
    void coro_worker(CoroYield &yield, RequstGen *gen, int coro_id);
    ~DpuProcesser();
};

DpuProcesser::DpuProcesser(/* args */) {

    using namespace std::placeholders;

    assert(coro_cnt <= define::kMaxCoro);
    for (int i = 0; i < coro_cnt; ++i) {
        auto gen = func(i, dsm, id);
        worker[i] = CoroCall(DpuProcesser::bind(&DpuProcesser::coro_worker, this, _1, gen, i));
    }

    master = CoroCall(std::bind(&Tree::coro_master, this, _1, coro_cnt));

    master();

}

void DpuProcesser::run_coroutine(CoroFunc func, int id, int coro_cnt) {

}

void DpuProcesser::coro_worker(CoroYield &yield, RequstGen *gen, int coro_id) {

}

DpuProcesser::~DpuProcesser() {

}





#endif