#include "Connection.h"
#include "Common.h"
#include <iostream>
#include "DpuCache.h"
#include "DpuProcessor.h"
#include "Timer.h"
#include <thread>
#include <stdio.h>
void func() {
    Timer t;
    t.begin();
    for (int i = 0; i < 1000000; i++) {
        char* a = (char*)malloc(1024);
        a[512] = 'a'; 
        a[128] = ';';
    } 
    t.end();
    std::cout << t.end() << std::endl;
    sleep(3);
}
int main() {
    // std::cout << sizeof(DpuCacheEntry) << std::endl;
    // std::cout << sizeof(InternalPage) << std::endl;
    printf("dadasdasdasdasdasd\n");
    sleep(10000000000);
    return 0;
}