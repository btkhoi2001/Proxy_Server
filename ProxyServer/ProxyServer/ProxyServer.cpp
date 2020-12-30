#include "ProxyServer.h"
#include <iostream>
#include <sstream>

namespace Network
{
    void ProxyServer::CloseConnection(int connectionIndex, std::string reason)
    {
        TCPConnection& connectionSocket = m_connections[connectionIndex];
        //std::cout << "[" << reason << "] Connection lost." << std::endl;
        m_master_fd.erase(m_master_fd.begin() + connectionIndex + 1);
        m_use_fd.erase(m_use_fd.begin() + connectionIndex + 1);
        connectionSocket.Close();
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

    bool ProxyServer::IsResponseHTML(const void* requestHTML)
    {
        std::string method((const char*)requestHTML, 4);

        if (method != "HTTP")
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
            if (m_listeningSocket.Listen(ip))
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

        if (WSAPoll(m_use_fd.data(), m_use_fd.size(), 1) > 0)
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
                    std::vector <char>& buffer = connection.m_incommingBuffer;

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
                        if (IsRequestHTML(&buffer[0]))
                        {
                            Socket newConnectionSocket;
                            Endpoint newConnectionEndpoint(GetHostNameFromRequest(&buffer[0]), 80);

                            newConnectionSocket.Create();
                            newConnectionSocket.Connect(newConnectionEndpoint);

                            TCPConnection newConnection(newConnectionSocket, newConnectionEndpoint);

                            newConnection.m_outgoingBuffer.resize(buffer.size());
                            memcpy_s(&newConnection.m_outgoingBuffer[0], buffer.size(), &buffer[0], buffer.size());

                            WSAPOLLFD newConnectionFD = {};
                            newConnectionFD.fd = newConnectionSocket.GetHandle();
                            newConnectionFD.events = POLLRDNORM;
                            newConnectionFD.revents = 0;

                            newConnection.Send();

                            int nbyteReceived;
                            while (WSAPoll(&newConnectionFD, 1, 1) >= 0)
                            {
                                if (newConnectionFD.revents & POLLRDNORM)
                                {
                                    nbyteReceived = newConnection.Recv();
                                    break;
                                }
                                continue;
                            }

                            std::vector<char>& newBuffer = newConnection.m_incommingBuffer;
                            connection.m_outgoingBuffer.resize(newBuffer.size());
                            memcpy_s(&connection.m_outgoingBuffer[0], newBuffer.size(), &newBuffer[0], newBuffer.size());

                            newConnection.Close();
                        }
                        else
                        {
                            CloseConnection(connectionIndex, "InvalidMethod");
                            continue;
                        }

                        for (auto const& c : buffer)
                        {
                            std::cout << c;
                        }
                    }

                }

                if (m_use_fd[i].revents & POLLWRNORM)
                {
                    connection.Send();
                    m_master_fd[i].events = POLLRDNORM;
                }
            }
        }
    }
}