#pragma once
#include "Socket.h"
#include <vector>

namespace Network
{
	class TCPConnection
	{
	private:
		Endpoint m_endpoint;
		Socket m_socket;
	public:
		TCPConnection(Socket socket, Endpoint endpoint);
		int Recv();
		int Send();
		void Close();
		std::vector<char> m_incommingBuffer;
		std::vector<char> m_outgoingBuffer;
	};
}