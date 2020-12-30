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
		Socket m_nextSocket;
	public:
		TCPConnection() {};
		TCPConnection(Socket socket, Endpoint endpoint, Socket m_nextSocket = Socket());
		int Recv();
		int Send();
		void Close();
		Socket GetSocket();
		Socket GetNextSocket();
		std::vector<char> m_incommingBuffer;
		std::vector<char> m_outgoingBuffer;
	};
}