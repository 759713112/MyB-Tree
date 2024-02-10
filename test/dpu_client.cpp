#include "Timer.h"
#include "Tree.h"
#include "zipf.h"
#include "DpuProxy.h"
#include "DpuProcessor.h"

#include <city.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <csignal>

//////////////////// workload parameters /////////////////////

// #define USE_CORO
const int kCoroCnt = 8;

int kReadRatio;
int kThreadCount;
int kNodeCount;
uint64_t kKeySpace = 64 * define::MB;
double kWarmRatio = 0.8;
double zipfan = 0;

//////////////////// workload parameters /////////////////////

extern uint64_t dpu_cache_miss[MAX_DPU_THREAD][8];
extern uint64_t dpu_cache_hit[MAX_DPU_THREAD][8];
extern uint64_t dpu_latency[MAX_DPU_THREAD][LATENCY_WINDOWS];
extern uint64_t dpu_tp[MAX_DPU_THREAD][8];

std::thread th[MAX_DPU_THREAD];
uint64_t tp[MAX_DPU_THREAD][8];


uint64_t latency_th_all[LATENCY_WINDOWS];

Tree *tree;
DpuProxy *dsm;
DpuProcessor *dpuProcessor;

Timer bench_timer;
std::atomic<int64_t> warmup_cnt{0};
std::atomic_bool ready{false};
void thread_run(int id) {

  bindCore(id);

  warmup_cnt.fetch_add(1);
  dsm->registerThread();
  if (id == 0) {
    while (warmup_cnt.load() != kThreadCount);
    ready = true;
    warmup_cnt.store(0);
  }
  printf("I am thread %d on dpu nodes\n", dsm->getMyThreadID());

  while (warmup_cnt.load() != 0)
    ;


#ifdef USE_CORO
  dpuProcessor->run_coroutine(id, kCoroCnt);
#else

  /// without coro

  Timer timer;
  while (true) {

    uint64_t dis = 0;
    uint64_t key = 0;

    Value v;
    timer.begin();

    if (v % 100 < kReadRatio) { // GET

    } else {
      
      
    }

    auto us_10 = timer.end() / 100;
    if (us_10 >= LATENCY_WINDOWS) {
      us_10 = LATENCY_WINDOWS - 1;
    }
    latency[id][us_10]++;

    tp[id][0]++;
  }
#endif

}

void parse_args(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: ./benchmark kNodeCount kThreadCount\n");
    exit(-1);
  }

  kNodeCount = atoi(argv[1]);
  kThreadCount = atoi(argv[2]);

  printf("kNodeCount %d, kReadRatio %d, kThreadCount %d\n", kNodeCount,
         kReadRatio, kThreadCount);
}

void cal_latency() {
  uint64_t all_lat = 0;
  for (int i = 0; i < LATENCY_WINDOWS; ++i) {
    latency_th_all[i] = 0;
    for (int k = 0; k < MAX_DPU_THREAD; ++k) {
      latency_th_all[i] += dpu_latency[k][i];
    }
    all_lat += latency_th_all[i];
  }

  uint64_t th50 = all_lat / 2;
  uint64_t th90 = all_lat * 9 / 10;
  uint64_t th95 = all_lat * 95 / 100;
  uint64_t th99 = all_lat * 99 / 100;
  uint64_t th999 = all_lat * 999 / 1000;

  uint64_t cum = 0;
  for (int i = 0; i < LATENCY_WINDOWS; ++i) {
    cum += latency_th_all[i];

    if (cum >= th50) {
      printf("p50 %f\t", i / 10.0);
      th50 = -1;
    }
    if (cum >= th90) {
      printf("p90 %f\t", i / 10.0);
      th90 = -1;
    }
    if (cum >= th95) {
      printf("p95 %f\t", i / 10.0);
      th95 = -1;
    }
    if (cum >= th99) {
      printf("p99 %f\t", i / 10.0);
      th99 = -1;
    }
    if (cum >= th999) {
      printf("p999 %f\n", i / 10.0);
      th999 = -1;
      return;
    }
  }
}

void signalHandler(int signal) {
    std::cout << "Ctrl+C signal received. Exiting." << std::endl;
    // 这里可以执行一些清理工作
    std::exit(signal);
}


int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  DSMConfig config;
  config.machineNR = kNodeCount;
  dsm = DpuProxy::getInstance(config);
  dpuProcessor = new DpuProcessor(dsm);

  for (int i = 0; i < kThreadCount; i++) {
    th[i] = std::thread(thread_run, i);
  }
  std::cout << "init ok" << std::endl;

  std::signal(SIGINT, signalHandler);

  while (!ready.load())
    ;

  timespec s, e;
  uint64_t pre_tp = 0;

  int count = 0;

  clock_gettime(CLOCK_REALTIME, &s);
  while (true) {

    sleep(2);
    clock_gettime(CLOCK_REALTIME, &e);
    int microseconds = (e.tv_sec - s.tv_sec) * 1000000 +
                       (double)(e.tv_nsec - s.tv_nsec) / 1000;

    uint64_t all_tp = 0;
    for (int i = 0; i < MAX_DPU_THREAD; ++i) {
      all_tp += dpu_tp[i][0];
      printf("thread %d tp %lld\n", i, dpu_tp[i][0]);
    }
    uint64_t cap = all_tp - pre_tp;
    pre_tp = all_tp;

    uint64_t all = 0;
    uint64_t hit = 0;
    for (int i = 0; i < MAX_APP_THREAD; ++i) {
      all += (dpu_cache_hit[i][0] + dpu_cache_miss[i][0]);
      hit += dpu_cache_hit[i][0];
    }

    clock_gettime(CLOCK_REALTIME, &s);

    if (++count % 3 == 0 && dsm->getMyNodeID() == 0) {
      cal_latency();
    }

    double per_node_tp = cap * 1.0 / microseconds;
    printf("%d, throughput %.4f\n", dsm->getMyNodeID(), per_node_tp);
    printf("cache hit rate: %lf\n", hit * 1.0 / all);

  }

  return 0;
}