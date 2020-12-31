#include "IPEndpoint.h"
#include <iostream>
#include <WS2tcpip.h>

namespace Network
{
    Endpoint::Endpoint(std::string ip, unsigned short port)
    {
        m_port = port;
        in_addr addr;

        // Kiểm tra địa ip có hợp lệ
        if (inet_pton(AF_INET, ip.c_str(), &addr) == 1)
        {
            if (addr.S_un.S_addr != INADDR_NONE)
            {
                m_ipString = ip;
                m_hostname = ip;

                return;
            }
        }

        // Tìm địa chỉ ip bằng hostname
        addrinfo hints = {};
        addrinfo* hostinfo = nullptr;
        hints.ai_family = AF_INET;

        if (getaddrinfo(ip.c_str(), NULL, &hints, &hostinfo) == 0)
        {
            sockaddr_in* host_addr = reinterpret_cast<sockaddr_in*>(hostinfo->ai_addr);

            m_ipString.resize(16);
            inet_ntop(AF_INET, &host_addr->sin_addr, &m_ipString[0], 16);

            m_hostname = ip;

            freeaddrinfo(hostinfo);
            return;
        }

        std::cerr << "Invalid IPv4 address.";
        exit(EXIT_FAILURE);
    }

    Endpoint::Endpoint(sockaddr* addr)
    {
        sockaddr_in* addrv4 = reinterpret_cast<sockaddr_in*>(addr);
        m_port = ntohs(addrv4->sin_port);
        m_ipString.resize(16);
        inet_ntop(AF_INET, &addrv4->sin_addr, &m_ipString[0], 16);
        m_hostname = m_ipString;
    }

    sockaddr_in Endpoint::GetSockaddrIPv4()
    {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = inet_addr(m_ipString.c_str());
        addr.sin_port = htons(m_port);

        return addr;
    }
}