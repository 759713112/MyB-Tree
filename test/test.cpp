#include "Connection.h"
#include "Common.h"
#include <iostream>
#include "DpuCache.h"
#include "DpuProcessor.h"


int main() {
    std::cout << sizeof(DpuCacheEntry) << std::endl;
    std::cout << sizeof(InternalPage) << std::endl;

    return 0;
}