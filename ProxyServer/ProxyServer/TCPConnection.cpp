#include "TCPConnection.h"

namespace Network
{
	TCPConnection::TCPConnection(Socket socket, Endpoint endpoint)
	{
		m_socket = socket;
		m_endpoint = endpoint;
	}

	int TCPConnection::Recv()
	{
		m_incommingBuffer.clear();
		std::vector <char> data(1024 * 1024 * 10);
		int bytesReceived = m_socket.RecvAll(&data[0]);
		//for (int i = data.size() - 1; i >= 0; i--)
		//{
		//	if (data[i] == '\0') {
		//		data.erase(data.begin() + i);
		//	}
		//}
		if (bytesReceived > 0)
		{
			m_incommingBuffer.resize(bytesReceived);
			memcpy_s(&m_incommingBuffer[0], bytesReceived, &data[0], bytesReceived);
		}

		return bytesReceived;
	}

	int TCPConnection::Send()
	{
		int bytesSent = m_socket.SendAll(&m_outgoingBuffer[0], m_outgoingBuffer.size());
		m_outgoingBuffer.clear();

		return bytesSent;
	}

	void TCPConnection::Close()
	{
		m_socket.Close();
	}
}