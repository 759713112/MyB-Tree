#include "DpuProcessor.h"



uint64_t dpu_cache_miss[MAX_DPU_THREAD][define::kMaxCoro];
uint64_t dpu_cache_hit[MAX_DPU_THREAD][define::kMaxCoro];
uint64_t dpu_latency[MAX_DPU_THREAD][LATENCY_WINDOWS];
uint64_t dpu_tp[MAX_DPU_THREAD][define::kMaxCoro];


thread_local CoroCall DpuProcessor::worker[define::kMaxCoro];
thread_local CoroCall DpuProcessor::master;
thread_local std::queue<DpuRequest*> DpuProcessor::task_queue;

static thread_local Timer timer;

DpuProcessor::DpuProcessor(DpuProxy *dpuProxy) : dpuProxy(dpuProxy) {}

void DpuProcessor::run_coroutine(int id, int coro_cnt) {
    using namespace std::placeholders;

    assert(coro_cnt <= define::kMaxCoro);
    for (int i = 0; i < coro_cnt; ++i) {
        worker[i] = CoroCall(std::bind(&DpuProcessor::coro_worker, this, _1, i));
    }

    master = CoroCall(std::bind(&DpuProcessor::coro_master, this, _1, coro_cnt));

    master();
}

void DpuProcessor::coro_worker(CoroYield &yield, int coro_id) {
    CoroContext ctx;
    ctx.coro_id = coro_id;
    ctx.master = &master;
    ctx.yield = &yield;

    Timer coro_timer;
    auto thread_id = dpuProxy->getMyThreadID();
    int bias = 0;
    GlobalAddress addr;
    addr.offset = 0;
    while (true) {
        while (task_queue.empty()) {
          int num = dpuProxy->pollRpcDpu(task_queue, define::kMaxCoro);
          if (num == 0) {
            ctx.appendToWaitQueue();
          } else {
              break;
          }
        }
        coro_timer.begin();
        DpuRequest* req = task_queue.front();
        task_queue.pop();
        Debug::debugCur("Get request, key: %d, node_id: %d, thread_id: %d, mythread_id: %d", req->k, req->node_id, req->app_id, thread_id);
        
        auto buffer = (char*)dpuProxy->readByDmaFixedSize(addr, &ctx);
        // static char* buffer = new char[1024];
        dpuProxy->rpc_rsp_dpu(buffer, *req);

        dpu_tp[thread_id][0]++;
        // if (thread_id == 0 && coro_id == 0) 
        // Debug::notifyInfo("Read addr %ld, latency: %d, ok: %d, count: %d, coro_id: %d", addr.offset, us_10, ok, ++count, ctx.coro_id);
        int us_10;
        if (us_10 >= LATENCY_WINDOWS) {
            us_10 = LATENCY_WINDOWS - 1;
        }
        dpu_latency[thread_id][us_10]++;
        bias = (bias + 1) % (1024 * 512);
    }
}

void DpuProcessor::coro_master(CoroYield &yield, int coro_cnt) {

  for (int i = 0; i < coro_cnt; ++i) {
    yield(worker[i]);
  }

  while (true) {

    uint64_t next_coro_id;
    // Debug::notifyInfo("doing master");
    // if (CoroContext::getQueueSize() < coro_cnt) {
    //   Debug::notifyInfo("doing master");
    //   while (dpuProxy->poll_dma_cq_once(next_coro_id)) {
    //     yield(worker[next_coro_id]);
    //   }
    // }
    if (dpuProxy->poll_dma_cq_once(next_coro_id)) {
        yield(worker[next_coro_id]);
    }
    if (CoroContext::popWaitQueue(next_coro_id)) {
      yield(worker[next_coro_id]);
    }
  }
}




void DpuProcessor::run_coroutine_cache_test(int id, int coro_cnt) {
    using namespace std::placeholders;

    assert(coro_cnt <= define::kMaxCoro);
    for (int i = 0; i < coro_cnt; ++i) {
        worker[i] = CoroCall(std::bind(&DpuProcessor::coro_worker_cache_test, this, _1, i));
    }

    master = CoroCall(std::bind(&DpuProcessor::coro_master_cache_test, this, _1, coro_cnt));

    master();
}
#include <random>
thread_local std::random_device rd;
thread_local std::mt19937 gen(rd());  // 使用 Mersenne Twister 引擎
thread_local std::uniform_int_distribution<uint64_t> distribution(0, 1520 * 1024 - 1);

thread_local int count = 0;
void DpuProcessor::coro_worker_cache_test(CoroYield &yield, int coro_id) {
    CoroContext ctx;
    ctx.coro_id = coro_id;
    ctx.master = &master;
    ctx.yield = &yield;

    Timer coro_timer;
    auto thread_id = dpuProxy->getMyThreadID();
    GlobalAddress addr = GlobalAddress::Null(); 
    int bias = 0;
    while (true) {
        addr.offset = ( distribution(gen)) << 10;
        // addr.offset = (((thread_id * 8 + coro_id) * 1024 + bias) << 10);
        coro_timer.begin();
        auto buffer = (char*)dpuProxy->readByDmaFixedSize(addr, &ctx);
        // char* buffer = "just aaaaa";
        // ctx.appendToWaitQueue();
        // dpuProxy->DpuConnection()->get_message();
        auto us_10 = coro_timer.end();
        bool ok = true;
        dpu_tp[thread_id][0]++;
        // if (thread_id == 0 && coro_id == 0) 
        // Debug::notifyInfo("Read addr %ld, latency: %d, ok: %d, count: %d, coro_id: %d", addr.offset, us_10, ok, ++count, ctx.coro_id);
    
        if (us_10 >= LATENCY_WINDOWS) {
            us_10 = LATENCY_WINDOWS - 1;
        }
        dpu_latency[thread_id][us_10]++;
        bias = (bias + 1) % (1024* 512);
    }
}

void DpuProcessor::coro_master_cache_test(CoroYield &yield, int coro_cnt) {

  for (int i = 0; i < coro_cnt; ++i) {
    yield(worker[i]);
  }

  while (true) {
    uint64_t next_coro_id  = 1000;
    
    if (dpuProxy->poll_dma_cq_once(next_coro_id)) {
      yield(worker[next_coro_id]);
    }

    if (CoroContext::popWaitQueue(next_coro_id)) {
        yield(worker[next_coro_id]);
    }
  }
}




DpuProcessor::~DpuProcessor() {

}
