#pragma once
#define WIN32_LEAN_AND_MEAN
#include "IPEndpoint.h"
#include <WinSock2.h>

#pragma comment(lib,"ws2_32.lib")

namespace Network
{
	class Socket
	{
	private:
		SOCKET m_handle = INVALID_SOCKET;
		bool SetSocketOption();
	public:
		Socket(SOCKET handle = INVALID_SOCKET);
		bool Create();
		bool Close();
		bool Bind(Endpoint endpoint);
		bool Listen(Endpoint endpoint, int backlog = 5);
		bool Accept(Socket& outSocket, Endpoint* endpoint = nullptr);
		bool Connect(Endpoint endpoint);
		int Send(const void* data, int numberOfBytes);
		int Recv(void* dataDestination, int numberOfBytes);
		int SendAll(const void* data, int numberOfBytes);
		int RecvAll(void* dataDestination);
		bool SetBlocking(bool isBlocking);
		SOCKET GetHandle();
	};
}