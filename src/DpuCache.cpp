#include "DpuCache.h"
#include <cmath>
#include "Timer.h"

DpuCacheMultiCon::DpuCacheMultiCon(FetchFunc f, void* buffer, size_t buffer_size, size_t page_size): 
        fetchFunc(f), buffer(buffer) {
    this->fixed_page_size = 512;
    this->page_size_pow = 9;
    while (this->fixed_page_size < page_size) {
        fixed_page_size <<= 1;
        this->page_size_pow++;
    }
    uint32_t page_num = buffer_size / this->fixed_page_size;
    assert(page_num % 4 == 0);
    assert((page_num & (page_num - 1)) == 0);
    this->group_map_mask = (page_num >> 2) - 1;
    this->target_tag_shif_num = page_size_pow + std::log2(page_num >> 2);
    entryArray = (DpuCacheEntry*)hugePageAlloc(page_num * sizeof(DpuCacheEntry));

    for (int i = 0; i < page_num; i++) {
        auto temp = entryArray + i;
        temp->setToCold(0xFFFFFFFF);
    }

}

void* DpuCacheMultiCon::get(uint64_t addr, CoroContext *ctx) {
next:
    uint64_t index = ((addr >> page_size_pow) & group_map_mask) << 2;
    void* local_addr = (this->buffer + ((index) << page_size_pow));
    // fetchFunc(local_addr, addr, fixed_page_size, ctx);
    // return local_addr;
    uint32_t target_tag = (addr >> 16);
    DpuCacheEntry *blockAddr = this->entryArray + index;
    for(uint8_t i = 0; i < WAY_NUM; i++) {
        auto current = blockAddr[i].getStateTag();
        if (current.tag == target_tag) {
           while (current.state == DpuCacheEntry::LOADING) {  
                if (ctx != nullptr) {
                    ctx->appendToWaitQueue();
                }
                current = blockAddr[i].getStateTag();
                // assert(current.tag == target_tag);
                if (current.tag != target_tag) goto next;
           }
           return this->buffer + ((index + i) << page_size_pow);     
        }
    }
    //not found, to be head
    uint8_t randomSelect = (addr >> target_tag_shif_num) % WAY_NUM;
    for (uint8_t i = 0; i < WAY_NUM + 1; i++) {
        randomSelect = (randomSelect + 1) % WAY_NUM;
        auto current = blockAddr[randomSelect].getStateTag(); 
        if (current.state == DpuCacheEntry::COLD) {
            blockAddr[randomSelect].setToLoading(target_tag);
            void* local_addr = (this->buffer + ((index + randomSelect) << page_size_pow));
            fetchFunc(local_addr, addr, fixed_page_size, ctx);
            blockAddr[randomSelect].setToHot(target_tag);
            blockAddr[randomSelect].notifyAll();
            return local_addr;
        } else if (current.state == DpuCacheEntry::HOT) {
            blockAddr[randomSelect].setToCold(current.tag);
        }
    }
    if (ctx != nullptr) { (*ctx->yield)(*ctx->master); }
    goto next;
    Debug::notifyError("Failed to get CacheEntry, %o", index);
    assert(0);
}
