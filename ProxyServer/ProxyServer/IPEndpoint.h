#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WS2tcpip.h>
#include <string>

namespace Network
{
	class Endpoint
	{
	private:
		std::string m_hostname = "";
		std::string m_ipString = "";
		unsigned short m_port = 0;
	public:
		Endpoint() {};
		Endpoint(std::string ip, unsigned short port);
		Endpoint(sockaddr* addr);
		sockaddr_in GetSockaddrIPv4();
	};
}