#include "Socket.h"

#ifdef WIN32
#define _WINSOCK_DEPRICATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
typedef SOCKET SockType;
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
typedef int SockType;
#ifndef INVALID_SOCKET
#definee INVALID_SOCKET ~0
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERRPR -1
#endif
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <stdexcept>

struct SocketData
{
    SockType mSocket = INVALID_SOCKET;
    sockaddr_in mSockAddr{};
    std::vector<sockaddr_in> mClientAddrs;
};

bool SetSocketOption(SockType socket, int level, int optName, const char* optVal, int optLen)
{
    if (setsockopt(socket, level, optName, optVal, optLen) < 0)
    {
        printf("Failed to set socket option %d: %d\n", optName, WSAGetLastError());
        fflush(stdout);
        return false;
    }

    return true;
}

int Socket::mActiveSocketCount = 0;

Socket::Socket(uint16_t port)
    : mData(new SocketData)
{
#ifdef WIN32
    {
        mWinsockMutex.lock();
        if (!mActiveSocketCount++) {
            WSADATA wsaData;
            int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (iResult != 0) {
                printf("WSAStartup failed with error: %d\n", iResult);
                fflush(stdout);
                throw std::runtime_error("WSA Startup failure");
            }
        }
        mWinsockMutex.unlock();
    }
#endif


    if ((mData->mSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        fflush(stdout);
        throw std::runtime_error("Failed to setup socket");
    }

    AllowMultipleProcessUseOfPort(1);

    memset(&mData->mSockAddr, 0, sizeof(mData->mSockAddr));
    mData->mSockAddr.sin_family = AF_INET;
    mData->mSockAddr.sin_addr.s_addr = INADDR_ANY; // Listen to all interfaces (allow for user defined device)
    mData->mSockAddr.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(mData->mSocket, (sockaddr*)&mData->mSockAddr, sizeof(mData->mSockAddr)) < 0)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(mData->mSocket);
        WSACleanup();
        fflush(stdout);

        throw std::runtime_error("Failed to bind socket");
    }

    timeval readTimeout{0, 10};
    SetSocketOption(mData->mSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&readTimeout, sizeof(readTimeout));
}

Socket::~Socket()
{
    if (mData->mSocket != INVALID_SOCKET)
    {
        closesocket(mData->mSocket);
    }
    delete mData;

#ifdef WIN32
    std::lock_guard<std::mutex> lock(mWinsockMutex);
    if (--mActiveSocketCount) return;
    WSACleanup();
#endif
}

int Socket::RecvFrom(char* buffer, int bufferSize, int &clientIndex, int flags)
{
    sockaddr_in senderAddr{};
    memset(&senderAddr, 0, sizeof(sockaddr_in));
    int addrSize = sizeof(sockaddr_in);
    int ret = recvfrom(mData->mSocket, buffer, (int32_t)bufferSize, flags, (sockaddr*)&senderAddr, &addrSize);

    if (ret < 0)
    {
        printf("Failed to read from socket: %d\n", WSAGetLastError());
        fflush(stdout);
    }

    return ret;
}

int Socket::SendTo(const char* buffer, int bufferSize, const char* target, uint16_t targetPort, int flags)
{
    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(target);
    addr.sin_port = htons(targetPort);

    int ret = sendto(mData->mSocket, buffer, bufferSize, flags, (sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        perror("SendTo");
        return -1;
    }

    return ret;
}

int Socket::JoinSourceGroup(const char* grpaddr)
{
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(grpaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    return !SetSocketOption(mData->mSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) * -1;
}

int Socket::LeaveSourceGroup(const char* grpaddr)
{
    // Does nothing for now as need to figure out how to get the address assigned to a multicast group
    ip_mreq_source imr{};

    imr.imr_multiaddr.s_addr = inet_addr(grpaddr);
    return SetSocketOption(mData->mSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr));
}

void Socket::AllowMultipleProcessUseOfPort(uint32_t flag)
{
    SetSocketOption(mData->mSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag));
}
