#include "Connection.h"
#include "Common.h"
#include <iostream>
#include "DpuCache.h"
#include "DpuProcessor.h"
#include "Timer.h"
<<<<<<< HEAD
=======
#include <google/tcmalloc.h>
>>>>>>> 5fffdbf952ee24999fcded466bf36aebd3cbe40b
#include <thread>

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
<<<<<<< HEAD
#include "PageStructure.h"
int main() {
    // std::cout << sizeof(DpuCacheEntry) << std::endl;
    InternalPage* page = new InternalPage;
    int cnt = 40;
    for (int i = 0; i < cnt; i++) {
        page->records[i].key = i * 5;
    }
    // 0 5 10 15 20 25 30 35 40 45 50 60 65 
    Key k = 27;
    int left = 1, right = cnt;
    int x = 0;
    while (left < right) {
        int mid = (left + right) / 2;
        if (k < page->records[mid].key) {
            right = mid;
        } else {
            left = mid + 1;
        }
        x++;
    }
    std::cout << left << " " << right << " " <<x << std::endl; 
=======
int main() {
    // std::cout << sizeof(DpuCacheEntry) << std::endl;
    // std::cout << sizeof(InternalPage) << std::endl;
    for (int i = 0; i < 10; i++) {
        auto t = new std::thread(func);
    }
    sleep(20);
>>>>>>> 5fffdbf952ee24999fcded466bf36aebd3cbe40b
    return 0;
}