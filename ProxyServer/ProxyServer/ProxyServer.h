#pragma once
#include "Socket.h"
#include "IPEndpoint.h"
#include "Winsock.h"
#include "TCPConnection.h"
#include <string>
#include <vector>

namespace Network
{
	//const char* forbidden = "HTTP/1.1 403 Forbidden\r\n\r\n<HTML>\r\n<BODY>\r\n<H1>403 Forbidden</H1>\r\n<H2>You don't have permission to access this server</H2>\r\n</BODY></HTML>\r\n";

	class ProxyServer
	{
	private:
		Socket m_listeningSocket;
		std::vector <TCPConnection> m_connections;
		std::vector <WSAPOLLFD> m_master_fd;
		std::vector <WSAPOLLFD> m_use_fd;
		void CloseConnection(int connectionIndex, std::string reason);
		bool IsRequestHTML(const void* requestHTML);
		std::string GetHostNameFromRequest(const void* requestHTML);
	public:
		bool Initialize(Endpoint ip);
		void Frame();
	};
}