#ifndef __DPU_CACHE__
#define __DPU_CACHE__


#include <atomic>
#include <functional>
#include <DmaDpu.h>
#include "HugePageAlloc.h"
#include "Tree.h"

class DpuCache {
public:
    virtual void* get(uint64_t addr, CoroContext *ctx) = 0;
    virtual void clear() {};
};

struct DpuCacheEntry {

    enum CacheEntryStatus : uint8_t {
        LOADING,
        COLD,
        HOT,
    };
    struct StateTag {
        CacheEntryStatus state;
        uint32_t tag;  // 48 - 10 - 18 < 32
    };
    void notifyAll() {
        // CoroContext *head = wait;
        // wait = nullptr;
        // while (head != nullptr) {
        //     head = head->next;
        // }  
    }
    void reset() {
        static StateTag temp = {COLD, 0};
        st.store(temp);
        wait = nullptr;
    }
    void setToLoading(uint32_t tag) {
        StateTag temp = {LOADING, tag};
        st.store(temp);
        wait = nullptr;
    }
    void setToCold(uint32_t tag) {
        StateTag temp = {COLD, tag};
        st.store(temp);
    }
    void setToHot(uint32_t tag) {
        StateTag temp  = {HOT, tag};
        st.store(temp);
    }
    void appendToWaitList(CoroContext *ctx) {
        //CAS to head;
    }

    StateTag getStateTag() {
        return st.load();
    }
    
    std::atomic<StateTag> st;
    CoroContext *wait;
};


#define WAY_NUM 4

#define DPU_CACHE_MASK 0x3FFFF  //18位
class DpuCacheMultiCon: public DpuCache {
public:
    typedef std::function<void(void *buffer, uint64_t addr, size_t size, CoroContext*)> FetchFunc;
    //fixed_page_size buffer_size 为2的次方
    DpuCacheMultiCon(FetchFunc f, void* buffer, size_t buffer_size, size_t fixed_page_size);
    virtual void* get(uint64_t addr, CoroContext *ctx) override;
    
private:
    DpuCacheEntry *entryArray;
    size_t fixed_page_size;
    uint32_t page_size_pow;
    uint32_t group_map_mask;
    uint32_t target_tag_shif_num;
    void *buffer;
    FetchFunc fetchFunc;
};


#endif