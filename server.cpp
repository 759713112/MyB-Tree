#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <city.h>
int main() {
    
    uint64_t a = 2;
    for (uint64_t a = 1; a < 20; a++) {
            std::cout << CityHash64((char*)&a, sizeof(a)) << std::endl;
    }

    std::cout << "Message sent to all hosts on the network.\n";


    return 0;
}