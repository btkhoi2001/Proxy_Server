#include "Socket.h"

namespace Network
{
	bool Socket::SetSocketOption()
	{
		BOOL value = TRUE;
		if (setsockopt(m_handle, IPPROTO_TCP, TCP_NODELAY, (const char*)&value, sizeof(value)) != 0)
		{
			return false;
		}

		return true;
	}

	Socket::Socket(SOCKET handle)
	{
		m_handle = handle;
	}

	bool Socket::Create()
	{
		if (m_handle != INVALID_SOCKET)
		{
			return false;
		}

		m_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (m_handle == INVALID_SOCKET)
		{
			return false;
		}

		if (SetSocketOption() != true)
		{
			return false;
		}

		if (SetBlocking(false) != true)
		{
			return false;
		}

		return true;
	}

	bool Socket::Close()
	{
		if (m_handle == INVALID_SOCKET)
		{
			return false;
		}

		if (closesocket(m_handle) == 0)
		{
			return false;
		}

		m_handle = INVALID_SOCKET;
		return true;
	}

	bool Socket::Bind(Endpoint endpoint)
	{
		sockaddr_in addr = endpoint.GetSockaddrIPv4();

		if (bind(m_handle, (sockaddr*)(&addr), sizeof(sockaddr_in)) != 0)
		{
			return false;
		}

		return true;
	}

	bool Socket::Listen(Endpoint endpoint, int backlog)
	{
		Bind(endpoint);

		if (listen(m_handle, backlog) != 0)
		{
			return false;
		}

		return true;
	}

	bool Socket::Accept(Socket& outSocket, Endpoint* endpoint)
	{
		sockaddr_in addr = {};
		int len = sizeof(sockaddr_in);
		SOCKET acceptedConnectionHandle = accept(m_handle, (sockaddr*)(&addr), &len);

		if (acceptedConnectionHandle == INVALID_SOCKET)
		{
			return false;
		}

		if (endpoint != nullptr)
		{
			*endpoint = Endpoint((sockaddr*)&addr);
		}

		outSocket = Socket(acceptedConnectionHandle);
		return true;
	}

	bool Socket::Connect(Endpoint endpoint)
	{
		SetBlocking(true);
		sockaddr_in addr = endpoint.GetSockaddrIPv4();
		int result = connect(m_handle, (sockaddr*)(&addr), sizeof(sockaddr_in));
		int error = WSAGetLastError();
		if (result != 0)
		{
			return false;
		}
		SetBlocking(false);

		return true;
	}

	int Socket::Send(const void* data, int numberOfBytes)
	{
		int bytesSent = send(m_handle, (const char*)data, numberOfBytes, NULL);

		if (bytesSent == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}

		return bytesSent;
	}

	int Socket::Recv(void* dataDestination, int numberOfBytes)
	{
		int bytesReceived = recv(m_handle, (char*)dataDestination, numberOfBytes, NULL);

		if (bytesReceived == 0)
		{
			return 0;
		}

		if (bytesReceived == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}

		return bytesReceived;
	}

	int Socket::SendAll(const void* data, int numberOfBytes)
	{
		int totalBytesSent = 0;
		while (true)
		{
			char* bufferOffset = (char*)data + totalBytesSent;
			int result = Send(bufferOffset, numberOfBytes - totalBytesSent);

			if (result == SOCKET_ERROR)
			{
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					return SOCKET_ERROR;
				}
				else
				{
					return totalBytesSent;
				}
			}

			if (result == 0)
			{
				return totalBytesSent;
			}

			totalBytesSent += result;
		}
	}

	int Socket::RecvAll(void* dataDestination)
	{
		int totalBytesReceived = 0;

		while (true)
		{
			char* bufferOffset = (char*)dataDestination + totalBytesReceived;
			int result = Recv(bufferOffset, 256);

			if (result == SOCKET_ERROR)
			{
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					return SOCKET_ERROR;
				}
				else
				{
					return totalBytesReceived;
				}
			}

			if (result == 0)
			{
				return totalBytesReceived;
			}

			totalBytesReceived += result;
		}
	}

	bool Socket::SetBlocking(bool isBlocking)
	{
		unsigned long nonBlocking = 1;
		unsigned long blocking = 0;

		if (ioctlsocket(m_handle, FIONBIO, isBlocking ? &blocking : &nonBlocking) == SOCKET_ERROR)
		{
			return false;
		}

		return true;
	}

	SOCKET Socket::GetHandle()
	{
		return m_handle;
	}
}