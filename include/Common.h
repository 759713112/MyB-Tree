#ifndef __COMMON_H__
#define __COMMON_H__

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <atomic>
#include <bitset>
#include <limits>

#include "Debug.h"
#include "HugePageAlloc.h"
#include "Rdma.h"

#include "WRLock.h"

// CONFIG_ENABLE_EMBEDDING_LOCK and CONFIG_ENABLE_CRC
// **cannot** be ON at the same time

// #define CONFIG_ENABLE_EMBEDDING_LOCK
// #define CONFIG_ENABLE_CRC

#define LATENCY_WINDOWS 1000000

#define STRUCT_OFFSET(type, field)                                             \
  (char *)&((type *)(0))->field - (char *)((type *)(0))

#define MAX_MACHINE 8

#define ADD_ROUND(x, n) ((x) = ((x) + 1) % (n))

#define MESSAGE_SIZE 1124 // byte

#define POST_RECV_PER_RC_QP 128

#define RAW_RECV_CQ_COUNT 128

// { app thread
#define MAX_APP_THREAD 8
#define MAX_DPU_THREAD 16

#define APP_MESSAGE_NR 96

// }

// { dir thread
#define NR_DIRECTORY 1

#define DIR_MESSAGE_NR 128

#define NODE_ID_FOR_DPU 4096
// }

#define SIGNAL_BATCH 31

void bindCore(uint16_t core);
char *getIP();
char *getMac();

inline int bits_in(std::uint64_t u) {
  auto bs = std::bitset<64>(u);
  return bs.count();
}

#include <boost/coroutine/all.hpp>

using CoroYield = boost::coroutines::symmetric_coroutine<void>::yield_type;
using CoroCall = boost::coroutines::symmetric_coroutine<void>::call_type;

struct CoroContext {
  CoroYield *yield;
  CoroCall *master;
  int coro_id;
  void appendToWaitQueue() {
    this->wait_queue.push(this->coro_id);
    (*this->yield)(*this->master);
  }
  static bool popWaitQueue(uint64_t &next_coro_id) {
    if (wait_queue.size() == 0) return false;
    next_coro_id = wait_queue.front();
    wait_queue.pop();
    return true;
  }
  static int getQueueSize() { return wait_queue.size(); }
  static thread_local std::queue<uint16_t> wait_queue;
private:
  

};


namespace define {

constexpr uint64_t MB = 1024ull * 1024;
constexpr uint64_t GB = 1024ull * MB;
constexpr uint16_t kCacheLineSize = 64;

// for remote allocate
constexpr uint64_t kChunkSize = 32 * MB;

// for store root pointer
constexpr uint64_t kRootPointerStoreOffest = kChunkSize / 2;
static_assert(kRootPointerStoreOffest % sizeof(uint64_t) == 0, "XX");

// lock on-chip memory
constexpr uint64_t kLockStartAddr = 0;
constexpr uint64_t kLockChipMemSize = 32 * 1024;

// number of locks
// we do not use 16-bit locks, since 64-bit locks can provide enough concurrency.
// if you want to use 16-bit locks, call *cas_dm_mask*
constexpr uint64_t kNumOfLock = kLockChipMemSize / sizeof(uint64_t);

// level of tree
constexpr uint64_t kMaxLevelOfTree = 7;

constexpr uint16_t kMaxCoro = 12;
constexpr int64_t kPerCoroRdmaBuf = 128 * 1024;

constexpr uint8_t kMaxHandOverTime = 8;

constexpr int kIndexCacheSize = 50; // MB
} // namespace define

//高精度时间戳
#ifndef AARCH64
static inline unsigned long long asm_rdtsc(void) {
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}
#else
static inline unsigned long long asm_rdtsc(void) {
  // unsigned hi, lo;
  // asm __volatile("rdtsc" : "=a"(lo), "=d"(hi));
  unsigned long long freq;   
	asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));   
  return freq; 
  return 0;
}

#endif

// For Tree
using Key = uint64_t;
using Value = uint64_t;
constexpr Key kKeyMin = std::numeric_limits<Key>::min();
constexpr Key kKeyMax = std::numeric_limits<Key>::max();
constexpr Value kValueNull = 0;

// Note: our RNICs can read 1KB data in increasing address order (but not for 4KB)
constexpr uint32_t kInternalPageSize = 1024;
constexpr uint32_t kLeafPageSize = 1024;

#ifndef AARCH64
__inline__ unsigned long long rdtsc(void) {
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}
#else
__inline__ unsigned long long rdtsc(void) {
  unsigned long long freq;   
	asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));   
  return freq; 
  return 0;
}
#endif


inline void mfence() { asm volatile("mfence" ::: "memory"); }

inline void compiler_barrier() { asm volatile("" ::: "memory"); }


//for doca dma
#define DMA_PCIE_ADDR "b5:00.0"
#define DMA_PCIE_ADDR_ON_DPU "03:00.0"
#define DMA_PCIE_ADDR_ON_HOST "b5:00.0"
#define MAX_DOCA_BUFS 512

#define DPU_CACHE_INTERNAL_PAGE_NUM 1048576 //2 ^20 
#endif /* __COMMON_H__ */
