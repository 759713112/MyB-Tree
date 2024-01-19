#include "Connection.h"
#include "Common.h"
#include <iostream>
#include "DpuCache.h"
int main() {
    std::cout << sizeof(DpuCacheEntry) << std::endl;
    std::cout << sizeof(InternalPage) << std::endl;

    InternalPage *a = 0;
    int i = 1;
    a += i;
    std::cout << a << std::endl;
    return 0;
}