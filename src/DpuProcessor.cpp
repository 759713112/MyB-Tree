#include "DpuProcessor.h"



uint64_t dpu_cache_miss[MAX_DPU_THREAD][8];
uint64_t dpu_cache_hit[MAX_DPU_THREAD][8];
uint64_t dpu_latency[MAX_DPU_THREAD][LATENCY_WINDOWS];
uint64_t dpu_tp[MAX_DPU_THREAD][8];


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
    DpuSearchResult result;

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

        auto curPageGlobalAddr = get_root_ptr(&ctx, coro_id);
        DpuSearchResult result;
        while (true) {
          page_search(curPageGlobalAddr, req->k, result, &ctx, coro_id);
          if (result.slibing == GlobalAddress::Null()) {
            if (result.level == 1) {
              break;
            }
            curPageGlobalAddr = result.next_level;
          } else {
            curPageGlobalAddr = result.slibing;
          }
        }
        
        // static char* buffer = new char[1024];
        // dpuProxy->rpc_rsp_dpu(buffer, *req);
        dpuProxy->rpc_rsp_dpu(result.cur_page, *req);

        dpu_tp[thread_id][0]++;
        // if (thread_id == 0 && coro_id == 0) 
        // Debug::notifyInfo("Read addr %ld, latency: %d, ok: %d, count: %d, coro_id: %d", addr.offset, us_10, ok, ++count, ctx.coro_id);
        int us_10;
        if (us_10 >= LATENCY_WINDOWS) {
            us_10 = LATENCY_WINDOWS - 1;
        }
        dpu_latency[thread_id][us_10]++;
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
        auto buffer = (char*)dpuProxy->readWithoutCache(addr, &ctx);
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

DpuProcessor::~DpuProcessor() {}

extern GlobalAddress g_root_ptr;
extern int g_root_level;
extern bool enable_cache;
GlobalAddress DpuProcessor::get_root_ptr(CoroContext *cxt, int coro_id) {
  if (g_root_ptr == GlobalAddress::Null()) {
    Debug::notifyError("cannot not get root ptr");
    exit(-1);
  } else {
    return g_root_ptr;
  }
}

extern InternalPage* root_node;
InternalPage* DpuProcessor::get_root_node() {
  assert(root_node != nullptr);
  return root_node;
}

thread_local GlobalAddress path_stack[define::kMaxCoro]
                                     [define::kMaxLevelOfTree];


bool DpuProcessor::page_search(GlobalAddress page_addr, const Key &k,
                       DpuSearchResult &result, CoroContext *ctx, int coro_id) {
  
  int counter = 0;
re_read:
  if (++counter > 100) {
    printf("re read too many times\n");
    sleep(1);
  }
  auto page = (InternalPage*)dpuProxy->readWithoutCache(page_addr, ctx);
  auto header = (Header *)((void*)page + (STRUCT_OFFSET(LeafPage, hdr)));
  memset(&result, 0, sizeof(result));
  result.level = header->level;
  result.cur_page = page;
  path_stack[coro_id][result.level] = page_addr;
  
  assert(result.level >= 1);
  
  if (!page->check_consistent()) {
      goto re_read;
  }
  if (k >= page->hdr.highest) { // should turn right
      result.slibing = page->hdr.sibling_ptr;
      return true;
  }
  if (k < page->hdr.lowest) {
      printf("key %ld error in level %d\n", k, page->hdr.level);
      assert(false);
      return false;
  }
  internal_page_search(page, k, result);
  return true;
}

void DpuProcessor::internal_page_search(InternalPage *page, const Key &k,
                                DpuSearchResult &result) {

  assert(k >= page->hdr.lowest);
  assert(k < page->hdr.highest);

  auto cnt = page->hdr.last_index + 1;
  // page->debug();
  if (k < page->records[0].key) {
    result.next_level = page->hdr.leftmost_ptr;
    return;
  }

  for (int i = 1; i < cnt; ++i) {
    if (k < page->records[i].key) {
      result.next_level = page->records[i - 1].ptr;
      return;
    }
  }
  result.next_level = page->records[cnt - 1].ptr;
}
