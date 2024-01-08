#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        std::cerr << "Error creating socket\n";
        return -1;
    }

    int broadcastEnable = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1) {
        std::cerr << "Error setting socket options\n";
        close(udpSocket);
        return -1;
    }

    struct sockaddr_in serverAddress, clientAddress;
    std::memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345); // Port number
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket\n";
        close(udpSocket);
        return -1;
    }

    char buffer[1024];
    socklen_t clientAddressLength = sizeof(clientAddress);

    ssize_t receivedBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddress, &clientAddressLength);
    if (receivedBytes == -1) {
        std::cerr << "Error receiving message\n";
        close(udpSocket);
        return -1;
    }

    buffer[receivedBytes] = '\0';
    std::cout << "Received message from " << inet_ntoa(clientAddress.sin_addr) << ": " << buffer << "\n";

    close(udpSocket);

    return 0;
}