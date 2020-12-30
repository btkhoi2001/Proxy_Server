#include "Winsock.h"
#include "Socket.h"
#include "IPEndpoint.h"
#include "ProxyServer.h"
#include <iostream>
#include <vector>

int main() {

	if (Network::Winsock::Initialize())
	{
		std::cout << "Winsock api successfully intialized." << std::endl;
		Network::ProxyServer proxyServer;
		if (proxyServer.Initialize(Network::Endpoint("localhost", 8888)))
		{
			while (true)
			{
				proxyServer.Frame();
			}
		}
	}
	else
	{
		std::cerr << "WSAStartup failed.";
		return EXIT_FAILURE;
	}

	Network::Winsock::Shutdown();

	return EXIT_SUCCESS;
}