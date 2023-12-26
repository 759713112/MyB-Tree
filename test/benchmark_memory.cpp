#include "Timer.h"
#include "Tree.h"
#include "zipf.h"
#include "DSMemory.h"

#include <city.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <csignal>

//////////////////// workload parameters /////////////////////

// #define USE_CORO

Tree *tree;
DSMemory *dsm;


int kNodeCount;
int kComputeCount;
void parse_args(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: ./benchmark_memory kNodeCount kComputeCount\n");
    exit(-1);
  }

  kNodeCount = atoi(argv[1]);
  kComputeCount = atoi(argv[2]);
  printf("kNodeCount %d, kComputeCount %d\n", kNodeCount, kComputeCount);
}


void signalHandler(int signal) {
    std::cout << "Ctrl+C signal received. Exiting." << std::endl;
    // 这里可以执行一些清理工作
    std::exit(signal);
}

int main(int argc, char *argv[]) {

  parse_args(argc, argv);

  DSMConfig config;
  config.machineNR = kComputeCount;
  // config.memoryNR = kComputeCount;
  config.isMemoryNode = true;
  dsm = DSMemory::getInstance(config);

  // tree = new Tree(dsm);

  // if (dsm->getMyNodeID() == 0) {
  //   for (uint64_t i = 1; i < 10; ++i) {
  //     tree->insert(to_key(i), i * 2);
  //   }
  // }

  // dsm->barrier("benchmark");

  std::signal(SIGINT, signalHandler);
  while(true) {
    sleep(10000);
  }

  return 0;
}