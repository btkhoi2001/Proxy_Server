#include "Winsock.h"
#include <iostream>

namespace Network
{
    bool Winsock::Initialize()
    {
        WORD wVersionRequested;
        WSADATA wsadata;
        int err;

        wVersionRequested = MAKEWORD(2, 2);
        err = WSAStartup(wVersionRequested, &wsadata);

        if (err != 0)
        {
            return false;
        }

        return true;
    }

    void Winsock::Shutdown()
    {
        WSACleanup();
    }
}