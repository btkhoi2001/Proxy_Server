#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#pragma comment(lib,"ws2_32.lib")

namespace Network
{
	class Winsock
	{
	public:
		static bool Initialize();
		static void Shutdown();
	};
}
