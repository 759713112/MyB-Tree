#include "Connection.h"
#include "Common.h"
#include <iostream>
int main() {
    RdmaContext ctx;
    
    createContext(&ctx);
    RdmaContext ctx2;
    
    createContext(&ctx2);
    uint8_t gid[16];
        memcpy((char *) gid, (char *)(&ctx.gid),
           16 * sizeof(uint8_t));
    struct ibv_ah_attr ahAttr;
    fillAhAttr(&ahAttr, ctx.lid, gid, &ctx);
    auto i = ibv_create_ah(ctx.pd, &ahAttr);

    std::cout << &ctx2 << std::endl;
    if (i == nullptr) {
        std::cout << "error" << std::endl;
        exit(-1);
        return -1;
    }
    return 0;

}