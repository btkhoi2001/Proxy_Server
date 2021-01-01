#pragma once
#include "Socket.h"
#include "IPEndpoint.h"
#include "Winsock.h"
#include "TCPConnection.h"
#include <string>
#include <vector>

namespace Network
{
	extern const char* forbiddenHTML;
	extern const char* blacklist;

	class ProxyServer
	{
	private:
		Socket m_listeningSocket;
		std::vector <TCPConnection> m_connections; 
		std::vector <WSAPOLLFD> m_master_fd;
		std::vector <WSAPOLLFD> m_use_fd;
		std::vector <std::string> m_blacklist;
		void LoadBlackList();
		bool IsBlacklisted(std::string hostname);
		bool IsAvailableHTML(const void* requestHTML);
		std::string GetHostnameFromRequest(const void* requestHTML);
		void CloseConnection(int connectionIndex, std::string reason);
	public:
		bool Initialize(Endpoint ip);
		void Frame();
	};
}