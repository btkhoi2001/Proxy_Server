#include "Winsock.h"
#include "ProxyServer.h"
#include <iostream>

int main() {

	// Khởi tạo thư viện winsock
	if (Network::Winsock::Initialize())
	{
		std::cout << "Winsock api successfully intialized." << std::endl;
		Network::ProxyServer proxyServer;

		// Khởi tạo proxy server
		if (proxyServer.Initialize(Network::Endpoint("localhost", 8888)))
		{
			// Vòng lặp chính sử dùng hàm WSAPoll để xử lý nhiều kết nối
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

	// Tắt thư viện winsock
	Network::Winsock::Shutdown();

	return EXIT_SUCCESS;
}