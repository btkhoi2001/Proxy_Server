#include "ProxyServer.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace Network
{
    const char* forbiddenHTML = "HTTP/1.1 403 Forbidden\r\n\r\n<HTML>\r\n<BODY>\r\n<H1>403 Forbidden</H1>\r\n<H2>You don't have permission to access this server</H2>\r\n</BODY></HTML>\r\n";
    const char* blacklist = "blacklist.conf";
    const char* crlf2 = "\r\n\r\n";

    void ProxyServer::LoadBlackList()
    {
        std::ifstream fileInput;
        fileInput.open(blacklist);

        while (true)
        {
            std::string name;
            if (std::getline(fileInput, name))
            {
                m_blacklist.push_back(name);
            }
            else
            {
                break;
            }
        }

        fileInput.close();
    }

    bool ProxyServer::IsBlacklisted(std::string hostname)
    {
        for (int i = 0; i < m_blacklist.size(); i++)
        {
            if (m_blacklist[i].find(hostname) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    bool ProxyServer::Initialize(Endpoint ip)
    {
        LoadBlackList();

        m_master_fd.clear();
        m_connections.clear();
        m_listeningSocket = Socket();

        if (m_listeningSocket.Create())
        {
            std::cout << "Socket successfully created." << std::endl;
            if (m_listeningSocket.Listen(ip, 10))
            {
                WSAPOLLFD listeningSocketFD = {};
                listeningSocketFD.fd = m_listeningSocket.GetHandle();
                listeningSocketFD.events = POLLRDNORM;
                listeningSocketFD.revents = 0;

                m_master_fd.push_back(listeningSocketFD);

                std::cout << "Socket successfully listening." << std::endl;
                return true;
            }
            else
            {
                std::cerr << "Failed to listen.";
            }
        }
        else
        {
            std::cerr << "Failed to create socket.";
        }

        return false;
    }

    void ProxyServer::Frame()
    {
        // Nếu outgoingBuffer chứa dữ liệu thì set thêm flag POLLWRNORM cho socket đó
        for (int i = 0; i < m_connections.size(); i++)
        {
            if (!m_connections[i].m_outgoingBuffer.empty())
            {
                m_master_fd[i + 1].events = POLLRDNORM | POLLWRNORM;
            }
        }

        m_use_fd = m_master_fd;

        // Kiểm tra trạng thái phản hồi từ các Socket
        if (WSAPoll(m_use_fd.data(), m_use_fd.size(), -1) > 0)
        {
#pragma region listener
            WSAPOLLFD& listeningSocketFD = m_use_fd[0];

            // Nếu có socket mới kết nối đến listening socket
            // Thêm kết nối đó vào danh sách
            if (listeningSocketFD.revents & POLLRDNORM)
            {
                Socket newConnectionSocket;
                Endpoint newConnectionEndpoint;
                if (m_listeningSocket.Accept(newConnectionSocket, &newConnectionEndpoint))
                {
                    m_connections.emplace_back(TCPConnection(newConnectionSocket, newConnectionEndpoint));

                    TCPConnection& acceptedConnection = m_connections[m_connections.size() - 1];
                    std::cout << "New connection accept." << std::endl;

                    WSAPOLLFD newConnectionFD = {};
                    newConnectionFD.fd = newConnectionSocket.GetHandle();
                    newConnectionFD.events = POLLRDNORM;
                    newConnectionFD.revents = 0;
                    m_master_fd.push_back(newConnectionFD);
                }
                else
                {
                    std::cerr << "Failed to accept new connection." << std::endl;
                }
            }
#pragma endregion Code xử lý riêng listening socket

            for (int i = m_use_fd.size() - 1; i >= 1; i--)
            {
                int connectionIndex = i - 1;
                TCPConnection& connection = m_connections[connectionIndex];
                std::vector <char>& inBuffer = connection.m_incommingBuffer;
                std::vector <char>& outBuffer = connection.m_outgoingBuffer;

                // Có lỗi xảy ra
                if (m_use_fd[i].revents & POLLERR)
                {
                    CloseConnection(connectionIndex, "POLLERR");
                    continue;
                }

                // Bị mất kết nối hoặc bị huỷ
                if (m_use_fd[i].revents & POLLHUP)
                {
                    CloseConnection(connectionIndex, "POLLHUP");
                    continue;
                }

                // Socket không hợp lệ
                if (m_use_fd[i].revents & POLLNVAL)
                {
                    CloseConnection(connectionIndex, "POLLNVAL");
                    continue;
                }

                // Có thể nhận được dữ liệu
                // Web browser -> Proxy server
                if (m_use_fd[i].revents & POLLRDNORM)
                {
                    int bytesReceived = connection.Recv();

                    // Mất kết nối
                    // Không nhận được dữ liệu
                    if (bytesReceived == 0)
                    {
                        CloseConnection(connectionIndex, "Recv=0");
                        continue;
                    }

                    // Có lỗi xảy ra
                    if (bytesReceived == SOCKET_ERROR)
                    {
                        // WSAEWOULDBLOCK xảy ra do sử dụng non-blocking socket nên được xử lý riêng
                        // Nếu là lỗi khác thì đóng kết nối
                        int error = WSAGetLastError();
                        if (error != WSAEWOULDBLOCK)
                        {
                            CloseConnection(connectionIndex, "Recv<0");
                            continue;
                        }
                    }

                    // Nếu nhận được dữ liệu (request)
                    if (bytesReceived > 0)
                    {
                        // Kiểm tra nếu request là method GET hoặc POST
                        if (IsAvailableHTML(&inBuffer[0]))
                        {
                            // Kiểm tra hostname có trong blacklist
                            std::string hostname = GetHostnameFromRequest(&inBuffer[0]);
                            if (IsBlacklisted(hostname))
                            {
                                outBuffer.resize(strlen(forbiddenHTML));
                                memcpy_s(&outBuffer[0], outBuffer.size(), forbiddenHTML, strlen(forbiddenHTML));
                                while (!outBuffer.empty())
                                {
                                    connection.Send();
                                }
                                CloseConnection(connectionIndex, "Forbidden");
                                continue;
                            }

#ifdef _DEBUG
                            std::cout << "HTTP request method:" << std::endl;
                            auto itBody = std::search(inBuffer.begin(), inBuffer.end(), crlf2, crlf2 + strlen(crlf2));

                            for (auto it = inBuffer.begin(); it != itBody; it++)
                            {
                                std::cout << *it;
                            }
                            std::cout << "\r\n\r\n";
#endif 
                            // Tạo kết nối mới đến web server
                            Socket newConnectionSocket;
                            Endpoint newConnectionEndpoint(hostname, 80);

                            newConnectionSocket.Create();
                            newConnectionSocket.Connect(newConnectionEndpoint);

                            TCPConnection newConnection(newConnectionSocket, newConnectionEndpoint);
                            std::vector <char>& n_inBuffer = newConnection.m_incommingBuffer;
                            std::vector <char>& n_outBuffer = newConnection.m_outgoingBuffer;

                            n_outBuffer.resize(bytesReceived);
                            memcpy_s(&n_outBuffer[0], bytesReceived, &inBuffer[0], bytesReceived);


                            WSAPOLLFD newConnectionFD = {};
                            newConnectionFD.fd = newConnectionSocket.GetHandle();
                            newConnectionFD.events = POLLRDNORM | POLLWRNORM;
                            newConnectionFD.revents = 0;

                            // Kiểm tra trạng thái phản hồi từ Socket của web server
                            while (WSAPoll(&newConnectionFD, 1, 1000) > 0)
                            {
                                // Có lỗi xảy ra
                                // Bị mất kết nối hoặc bị huỷ
                                // Socket không hợp 
                                if (newConnectionFD.revents & POLLERR || newConnectionFD.revents & POLLHUP || newConnectionFD.revents & POLLNVAL)
                                {
                                    break;
                                }

                                // Có thể nhận được dữ liệu
                                // Web server -> Proxy Server
                                if (newConnectionFD.revents & POLLRDNORM)
                                {
                                    int n_bytesReceived = newConnection.Recv();

                                    // Mất kết nối
                                    // Không nhận được dữ liệu
                                    if (n_bytesReceived == 0)
                                    {
                                        break;
                                    }

                                    // Có lỗi xảy ra
                                    if (n_bytesReceived == SOCKET_ERROR)
                                    {
                                        // WSAEWOULDBLOCK xảy ra do sử dụng non-blocking socket nên được xử lý riêng
                                        // Nếu là lỗi khác thì đóng kết nối
                                        int error = WSAGetLastError();
                                        if (error != WSAEWOULDBLOCK)
                                        {
                                            break;
                                        }
                                    }

                                    // Nếu nhận được dữ liệu (response)
                                    if (n_bytesReceived > 0)
                                    {
                                        // Copy response vào outgoingBuffer của kết nối đến web browser
                                        int connectionSize = outBuffer.size();
                                        outBuffer.resize(connectionSize + n_bytesReceived);
                                        memcpy_s(&outBuffer[0] + connectionSize, n_bytesReceived, &n_inBuffer[0], n_bytesReceived);
                                    }
                                }

                                // Có thể gửi được dữ liệu (request)
                                // Proxy server -> Web server
                                if (newConnectionFD.revents & POLLWRNORM)
                                {
                                    while (!n_outBuffer.empty())
                                    {
                                        newConnection.Send();
                                    }
                                    newConnectionFD.events = POLLRDNORM;
                                }
                            }

                            // Đóng kết nối đến web Server
                            newConnection.Close();
                        }
                        // Method không hỗ trợ
                        else
                        {
                            CloseConnection(connectionIndex, "UnavailableMethod");
                            continue;
                        }
                    }
                }

                // Có thể gửi được dữ liệu (response)
                // Proxy server -> Web browser
                if (m_use_fd[i].revents & POLLWRNORM)
                {
#ifdef _DEBUG
                    std::cout << "HTTP response:" << std::endl; 
                    auto itBody = std::search(outBuffer.begin(), outBuffer.end(), crlf2, crlf2 + strlen(crlf2));

                    for (auto it = outBuffer.begin(); it != itBody; it++)
                    {
                        std::cout << *it;
                    }
                    std::cout << "\r\n\r\n";
#endif

                    while (!outBuffer.empty())
                    {
                        connection.Send();
                    }

                    // Sau khi gửi dữ liệu set lại flag POLLRDNORM cho socket đó
                    m_master_fd[i].events = POLLRDNORM;
                }
            }
        }
    }

    bool ProxyServer::IsAvailableHTML(const void* requestHTML)
    {
        std::string method((const char*)requestHTML, 4);

        if (method.substr(0, 3) != "GET" && method != "POST")
        {
            return false;
        }

        return true;;
    }

    std::string ProxyServer::GetHostnameFromRequest(const void* requestHTML)
    {
        std::stringstream request((const char*)requestHTML);
        std::string hostname;

        while (getline(request, hostname))
        {
            if (hostname.find("Host: ") != std::string::npos)
            {
                hostname.erase(0, strlen("Host: "));
                while (hostname.find('\r') != std::string::npos || hostname.find('\n') != std::string::npos)
                {
                    hostname.erase(hostname.end() - 1);
                }
                break;
            }
        }

        return hostname;
    }

    void ProxyServer::CloseConnection(int connectionIndex, std::string reason)
    {
        //std::cout << "[" << reason << "] Connection lost." << std::endl;
        TCPConnection& connection = m_connections[connectionIndex];
        connection.Close();
        m_master_fd.erase(m_master_fd.begin() + connectionIndex + 1);
        m_connections.erase(m_connections.begin() + connectionIndex);
    }
}