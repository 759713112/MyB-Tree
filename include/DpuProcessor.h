// #ifndef __DPU_PROCESSOR__
// #define __DPU_PROCESSOR__

// #include <queue>



// #include "Tree.h"
// #include "Timer.h"




// class DpuProcessor {

// private:
//     DpuProxy *dpuProxy;
//     static thread_local CoroCall worker[define::kMaxCoro];
//     static thread_local CoroCall master;
// public:
//     DpuProcessor(/* args */);
//     void run_coroutine(CoroFunc func, int id, int coro_cnt);
//     void coro_worker(CoroYield &yield, RequstGen *gen, int coro_id);
//     void coro_master(CoroYield &yield, int coro_cnt);
//     ~DpuProcessor();
// };


// uint64_t cache_miss[MAX_APP_THREAD][8];
// uint64_t cache_hit[MAX_APP_THREAD][8];
// uint64_t latency[MAX_APP_THREAD][LATENCY_WINDOWS];

// thread_local CoroCall DpuProcessor::worker[define::kMaxCoro];
// thread_local CoroCall DpuProcessor::master;
// thread_local GlobalAddress path_stack[define::kMaxCoro]
//                                      [define::kMaxLevelOfTree];

// thread_local Timer timer;

// DpuProcessor::DpuProcessor(/* args */) {



// }

// void DpuProcessor::run_coroutine(CoroFunc func, int id, int coro_cnt) {
//     using namespace std::placeholders;

//     assert(coro_cnt <= define::kMaxCoro);
//     for (int i = 0; i < coro_cnt; ++i) {
//         auto gen = func(i, dpuProxy, id);
//         worker[i] = CoroCall(std::bind(&DpuProcessor::coro_worker, this, _1, gen, i));
//     }

//     master = CoroCall(std::bind(&DpuProcessor::coro_master, this, _1, coro_cnt));

//     master();
// }

// void DpuProcessor::coro_worker(CoroYield &yield, RequstGen *gen, int coro_id) {
//     CoroContext ctx;
//     ctx.coro_id = coro_id;
//     ctx.master = &master;
//     ctx.yield = &yield;

//     // Timer coro_timer;
//     // auto thread_id = dpuProxy->getMyThreadID();

//     // while (true) {
//     //     auto r = gen->next();
//     //     coro_timer.begin();
//     //     if (r.is_search) {
//     //         Value v;
//     //         this->search(r.k, v, &ctx, coro_id);
//     //     } else {
//     //         this->insert(r.k, r.v, &ctx, coro_id);
//     //     }
//     //     auto us_10 = coro_timer.end() / 100;
//     //     if (us_10 >= LATENCY_WINDOWS) {
//     //         us_10 = LATENCY_WINDOWS - 1;
//     //     }
//     //     latency[thread_id][us_10]++;
//     // }
// }


// void DpuProcessor::coro_master(CoroYield &yield, int coro_cnt) {

//   // for (int i = 0; i < coro_cnt; ++i) {
//   //   yield(worker[i]);
//   // }

//   // while (true) {

//   //   uint64_t next_coro_id;

//   //   if (dpuProxy->poll_rdma_cq_once(next_coro_id)) {
//   //     yield(worker[next_coro_id]);
//   //   }

//   //   if (!hot_wait_queue.empty()) {
//   //     next_coro_id = hot_wait_queue.front();
//   //     hot_wait_queue.pop();
//   //     yield(worker[next_coro_id]);
//   //   }
//   // }
// }

// DpuProcessor::~DpuProcessor() {

// }





// #endif