#pragma once

#include <cstdint>
#include <mutex>

class Socket
{
public:
    explicit Socket(uint16_t port);
    virtual ~Socket();

    int RecvFrom(char* buffer, int bufferSize, int& clientIndex, int flags = 0);
    int SendTo(const char* buffer, int bufferSize, const char* target, uint16_t targetPort, int flags = 0);

    int JoinSourceGroup(const char* groupAddr);
    int LeaveSourceGroup(const char* groupAddr);
    void AllowMultipleProcessUseOfPort(uint32_t flag);

protected:
    struct SocketData* mData;
#ifdef WIN32
    std::mutex mWinsockMutex;
    static int mActiveSocketCount;
#endif
};