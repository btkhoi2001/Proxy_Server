#include "ProxyServer.h"
#include <iostream>
#include <sstream>
#include <fstream>

namespace Network
{
    void ProxyServer::CloseConnection(int connectionIndex, std::string reason)
    {
#ifdef _DEBUG
        std::cout << "[" << reason << "] Connection lost."<< std::endl;
#endif
        TCPConnection& connection = m_connections[connectionIndex];
        connection.Close();
        m_master_fd.erase(m_master_fd.begin() + connectionIndex + 1);
        m_connections.erase(m_connections.begin() + connectionIndex);
    }

    bool ProxyServer::IsRequestHTML(const void* requestHTML)
    {
        std::string method((const char*)requestHTML, 4);

        if (method.substr(0, 3) != "GET" && method != "POST")
        {
            return false;
        }

        return true;;
    }

    std::string ProxyServer::GetHostNameFromRequest(const void* requestHTML)
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

    bool ProxyServer::Initialize(Endpoint ip)
    {
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
        for (int i = 0; i < m_connections.size(); i++)
        {
            if (!m_connections[i].m_outgoingBuffer.empty())
            {
                m_master_fd[i + 1].events = POLLRDNORM | POLLWRNORM;
            }
        }

        m_use_fd = m_master_fd;

        if (WSAPoll(m_use_fd.data(), m_use_fd.size(), -1) > 0)
        {
#pragma region listener
            WSAPOLLFD& listeningSocketFD = m_use_fd[0];
            if (listeningSocketFD.revents & POLLRDNORM)
            {
                Socket newConnectionSocket;
                Endpoint newConnectionEndpoint;
                if (m_listeningSocket.Accept(newConnectionSocket, &newConnectionEndpoint))
                {
                    m_connections.emplace_back(TCPConnection(newConnectionSocket, newConnectionEndpoint));

                    TCPConnection& acceptedConnection = m_connections[m_connections.size() - 1];
                    //std::cout << "New connection accept." << std::endl;

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
#pragma endregion Code specific to the listening socket

            for (int i = m_use_fd.size() - 1; i >= 1; i--)
            {
                int connectionIndex = i - 1;
                TCPConnection& connection = m_connections[connectionIndex];

                if (m_use_fd[i].revents & POLLERR)
                {
                    CloseConnection(connectionIndex, "POLLERR");
                    continue;
                }

                if (m_use_fd[i].revents & POLLHUP)
                {
                    CloseConnection(connectionIndex, "POLLHUP");
                    continue;
                }

                if (m_use_fd[i].revents & POLLNVAL)
                {
                    CloseConnection(connectionIndex, "POLLNVAL");
                    continue;
                }

                if (m_use_fd[i].revents & POLLRDNORM)
                {
                    int bytesReceived = connection.Recv();
                    std::vector <char>& inBuffer = connection.m_incommingBuffer;
                    std::vector <char>& outBuffer = connection.m_outgoingBuffer;

                    if (bytesReceived == 0)
                    {
                        CloseConnection(connectionIndex, "Recv=0");
                        continue;
                    }

                    if (bytesReceived == SOCKET_ERROR)
                    {
                        int error = WSAGetLastError();
                        if (error != WSAEWOULDBLOCK)
                        {
                            CloseConnection(connectionIndex, "Recv<0");
                            continue;
                        }
                    }

                    if (bytesReceived > 0)
                    {
                        if (IsRequestHTML(&inBuffer[0]))
                        {
#ifdef _DEBUG
                            std::cout << "HTTP request method:" << std::endl;
                            for (auto const& c : inBuffer)
                            {
                                std::cout << c;
                            }
#endif

                            Socket newConnectionSocket;
                            Endpoint newConnectionEndpoint(GetHostNameFromRequest(&inBuffer[0]), 80);

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

                            while (WSAPoll(&newConnectionFD, 1, 1000) > 0)
                            {
                                if (newConnectionFD.revents & POLLERR || newConnectionFD.revents & POLLHUP || newConnectionFD.revents & POLLNVAL)
                                {
                                    break;
                                }

                                if (newConnectionFD.revents & POLLRDNORM)
                                {
                                    int n_bytesReceived = newConnection.Recv();

                                    if (n_bytesReceived == 0)
                                    {
                                        break;
                                    }

                                    if (n_bytesReceived == SOCKET_ERROR)
                                    {
                                        int error = WSAGetLastError();
                                        if (error != WSAEWOULDBLOCK)
                                        {
                                            break;
                                        }
                                    }

                                    if (n_bytesReceived > 0)
                                    {
                                        int connectionSize = outBuffer.size();
                                        outBuffer.resize(connectionSize + n_bytesReceived);
                                        memcpy_s(&outBuffer[0] + connectionSize, n_bytesReceived, &n_inBuffer[0], n_bytesReceived);
                                    }

#ifdef _DEBUG
                                    std::cout << "HTTP response:" << std::endl;
                                    for (auto const& c : n_inBuffer)
                                    {
                                        std::cout << c;
                                    }
#endif
                                }

                                if (newConnectionFD.revents & POLLWRNORM)
                                {
                                    while (!n_outBuffer.empty())
                                    {
                                        newConnection.Send();
                                    }
                                    newConnectionFD.events = POLLRDNORM;
                                }
                            }
                            newConnection.Close();
                        }
                        else
                        {
                            CloseConnection(connectionIndex, "InvalidMethod");
                            continue;
                        }
                    }
                }

                if (m_use_fd[i].revents & POLLWRNORM)
                {
                    while (!connection.m_outgoingBuffer.empty())
                    {
                        connection.Send();
                    }
                    m_master_fd[i].events = POLLRDNORM;
                }
            }
        }
    }
}