#ifndef __DPU_PROCESSOR__
#define __DPU_PROCESSOR__

#include <queue>
#include "PageStructure.h"
#include "Timer.h"
#include "Tree.h"
#include "DpuProxy.h"

struct DpuSearchResult {
  uint8_t level;
  GlobalAddress slibing;
  GlobalAddress next_level;
  InternalPage *cur_page;
};

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
private:
    GlobalAddress get_root_ptr(CoroContext *cxt, int coro_id);
    InternalPage* get_root_node();
    void internal_page_search(InternalPage *page, const Key &k, DpuSearchResult &result);
    bool page_search(GlobalAddress page_addr, const Key &k, DpuSearchResult &result, 
                    CoroContext *cxt, int coro_id);
};





#endif