#include <stdio.h>
#include <stdexcept>
#include <cassert>
#include "spdlog/spdlog.h"
#include "AsyncNetworkHandler.h"

namespace simplenet
{

AsyncNetworkHandler::AsyncNetworkHandler(unsigned short _usersPort, Callback _newConnectionFunc, Callback _closedConnectionFunc)
	: m_usersPort(_usersPort)
	, m_remoteServerPort(0)
	, m_totalSockets(0)
	, m_nextReadSocketToTraverse(0), m_totalSocketsReady(0)
	, m_newConnectionFunc(_newConnectionFunc)
	, m_closedConnectionFunc(_closedConnectionFunc)
{
	Init();
	FD_ZERO(&m_lastReadSet);
}

void AsyncNetworkHandler::InitWsa()
{
#ifdef WIN32
	static bool s_alreadyDone = false;
	if (!s_alreadyDone)
	{
		WSADATA wsaData;
		int ret = WSAStartup(0x0202,&wsaData);
		if (ret != 0)
		{
			printf("WSAStartup() failed with error %d\n", ret);
			WSACleanup();
			throw std::bad_alloc();
		}
		s_alreadyDone = true;
	}
#endif
}

#ifndef WIN32
#define WSASocket(A, B, C, D, E, F) socket(A, B , C);
inline int WSAGetLastError()
{
	return errno;
}
#endif

void AsyncNetworkHandler::InitListener(SOCKET *_socket, unsigned short _port)
{
	if ((*_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		spdlog::error("WSASocket() failed with error {}", WSAGetLastError());
		throw std::bad_alloc();
	}
	else
	{
		spdlog::debug("WSASocket() is OK!");
	}

	SOCKADDR_IN internetAddr;
	internetAddr.sin_family = AF_INET;
	internetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	internetAddr.sin_port = htons(_port);

   if (bind(*_socket, (PSOCKADDR) &internetAddr, sizeof(internetAddr)) == SOCKET_ERROR)
   {
		spdlog::error("bind() failed with error {}", WSAGetLastError());
		throw std::exception("bind() failed");
   }
   else
   {
		spdlog::debug("bind() is OK!");
   }

   if (listen(*_socket, 5))
   {
      spdlog::error("listen() failed with error {}", WSAGetLastError());
	  throw std::exception("listen() failed");
   }
   else
   {
       spdlog::debug("listen() is OK!");
   }

   // Change the socket mode on the listening socket from blocking to
   // non-block so the application will not block waiting for requests
   ULONG nonBlock = 1;
   if (ioctlsocket(*_socket, FIONBIO, &nonBlock) == SOCKET_ERROR)
   {
      spdlog::error("ioctlsocket() failed with error {}", WSAGetLastError());
	  throw std::exception("ioctlsocket() failed");
   }
   else
   {
      spdlog::debug("ioctlsocket() is OK!");
   }
}

void AsyncNetworkHandler::Init()
{
	InitWsa();
	InitListener(&m_listenSocket, m_usersPort);
}

void AsyncNetworkHandler::GetInput(size_t *_socketid, std::string *_input)
{
	while (!GetInput(_socketid, _input, 1))
		;
}

bool AsyncNetworkHandler::GetInput(size_t *_socketid, std::string *_input, unsigned long _timeoutMillisecs)
{
	if (m_socketArray[m_nextReadSocketToTraverse])
	{
		SOCKET_INFORMATION &socketInfo = *m_socketArray[m_nextReadSocketToTraverse];

		if (socketInfo.bufferMaybeReady)
		{
			// still have data in buffer from previous recv
			if (FillInputString(socketInfo, 0, _input))
			{
				*_socketid = m_nextReadSocketToTraverse;
				if (!socketInfo.bufferMaybeReady)
				{
					++m_nextReadSocketToTraverse;
				}
				return true;
			}
		}
	}

	while (true)
	{
		if (!m_totalSocketsReady)
		{
			FD_SET writeSet;
			FD_ZERO(&writeSet);
			FD_ZERO(&m_lastReadSet);
			m_nextReadSocketToTraverse = 0;

			// Always look for connection attempts
			FD_SET(m_listenSocket, &m_lastReadSet);

			// Set Read and Write notification for each socket based on the
			// current state the buffer.  If there is data remaining in the
			// buffer then set the Write set otherwise the Read set
			for (int i = 0 ; i < FD_SETSIZE ; i++)
			{
				if (m_socketArray[i])
				{
					if (m_socketArray[i]->writeLen) 
					{
						FD_SET(m_socketArray[i]->Socket, &writeSet);
					}
					else
					{
						FD_SET(m_socketArray[i]->Socket, &m_lastReadSet);
					}
				}
			}

			timeval t;
			t.tv_sec = _timeoutMillisecs / 1000;
			t.tv_usec = (_timeoutMillisecs % 1000) * 1000;
			m_totalSocketsReady = select(FD_SETSIZE, &m_lastReadSet, &writeSet, NULL, &t) ;
			if (m_totalSocketsReady == SOCKET_ERROR)
			{
				m_totalSocketsReady = 0;
				spdlog::error("select() returned with error {}", WSAGetLastError());
				throw std::exception("select() returned with error");
			}
			else if (!m_totalSocketsReady)
			{
				return false;
			}
			else 
			{
				assert(m_totalSocketsReady);
				spdlog::debug("select() is OK! {}", m_totalSocketsReady);
			}

			//**************************************************************SEND*******************
			// Check each socket for WRITE notification until the number
			for (int i = 0 ; m_totalSocketsReady && i < FD_SETSIZE ; i++)
			{
				if (!m_socketArray[i])
				{
					continue; // skip
				}

				SOCKET_INFORMATION &socketInfo = *m_socketArray[i];

				// If the WriteSet is marked on this socket then this means the internal
				// data buffers are available for more data
				if (FD_ISSET(socketInfo.Socket, &writeSet))
				{
					--m_totalSocketsReady;

					DWORD sendBytes = send(socketInfo.Socket, socketInfo.writeBuffer, socketInfo.writeLen, 0);
					if (sendBytes == SOCKET_ERROR)
					{
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							spdlog::error("WSASend() failed with error {}", WSAGetLastError());
							FreeSocketInformation(i);
						}
						else
						{
							spdlog::debug("WSASend() is OK!");
						}

						continue;
					}
					else
					{
						assert(sendBytes);
						if (sendBytes < socketInfo.writeLen)
						{
							memcpy(socketInfo.writeBuffer, socketInfo.writeBuffer + sendBytes, socketInfo.writeLen - sendBytes);
							socketInfo.writeLen -= sendBytes;
						}
						else
						{
							socketInfo.writeLen = 0;
						}
					}
				}
			}

			//**************************************************************ACCEPT*******************
			// Check for arriving connections on the listening socket.
			if (FD_ISSET(m_listenSocket, &m_lastReadSet))
			{
				assert(m_totalSocketsReady > 0);
				--m_totalSocketsReady;
				SOCKET newSocket = accept(m_listenSocket, NULL, NULL);
				if (newSocket != INVALID_SOCKET)
				{
					// Set the accepted socket to non-blocking mode so the server will
					// not get caught in a blocked condition on WSASends
					ULONG NonBlock = 1;
					if (ioctlsocket(newSocket, FIONBIO, &NonBlock) == SOCKET_ERROR)
					{
						spdlog::error("ioctlsocket(FIONBIO) failed with error {}", WSAGetLastError());
						throw std::exception("ioctlsocket(FIONBIO) failed with error");
					}
					else
					{
						spdlog::debug("ioctlsocket(FIONBIO) is OK!");
					}

					*_socketid = CreateSocketInformation(newSocket);
					_input->clear();
					return true;
				}
				else
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						spdlog::error("accept() failed with error {}", WSAGetLastError());
						throw std::exception("accept() failed with error");
					}
					else
					{
						spdlog::debug("accept() is fine!");
					}
				}
			}
		}
		//**************************************************************RECV*******************
		for ( ; m_totalSocketsReady ; ++m_nextReadSocketToTraverse)
		{
			if (!m_socketArray[m_nextReadSocketToTraverse])
			{
				continue;
			}
			SOCKET_INFORMATION &socketInfo = *m_socketArray[m_nextReadSocketToTraverse];

			assert(!socketInfo.bufferMaybeReady);

			// If the ReadSet is marked for this socket then this means data
			// is available to be read on the socket
			if (FD_ISSET(socketInfo.Socket, &m_lastReadSet))
			{
				--m_totalSocketsReady;

				DWORD recvBytes = recv(socketInfo.Socket, &socketInfo.readBuffer[socketInfo.BytesRECV], READ_DATA_BUFSIZE - socketInfo.BytesRECV, 0);
				if (recvBytes == -1)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						spdlog::error("WSARecv() failed with error {}", WSAGetLastError());
						FreeSocketInformation(m_nextReadSocketToTraverse);
					}
					else
					{
						spdlog::debug("WSARecv() is OK!");
					}
					continue;
				}
				else
				{
					// If zero bytes are received, this indicates the peer closed the connection.
					if (recvBytes == 0)
					{
						FreeSocketInformation(m_nextReadSocketToTraverse);
						continue;
					}

					DWORD prevBytes = socketInfo.BytesRECV;
					socketInfo.BytesRECV += recvBytes;

					if (FillInputString(socketInfo, prevBytes, _input))
					{
						*_socketid = m_nextReadSocketToTraverse;
						if (!socketInfo.bufferMaybeReady)
						{
							++m_nextReadSocketToTraverse;
						}
						return true;
					}
				}
			}
		}
		assert(!m_totalSocketsReady);
	}
}

bool AsyncNetworkHandler::FillInputString(SOCKET_INFORMATION &socketInfo, DWORD prevBytes, std::string *_input)
{
	assert(socketInfo.BytesRECV);
	static const char NEWLINE_MARKERS[] = { '\n', '\r' };
	static const char *NEWLINE_MARKERS_END = NEWLINE_MARKERS + sizeof(NEWLINE_MARKERS)/sizeof(NEWLINE_MARKERS[0]);
	char * const bufferEnd = socketInfo.readBuffer + socketInfo.BytesRECV;
	char *firstNewlinePos = std::find_first_of(socketInfo.readBuffer + prevBytes, bufferEnd, NEWLINE_MARKERS, NEWLINE_MARKERS_END);
	//if (socketInfo.readBuffer[socketInfo.BytesRECV - 1] == '\n' || socketInfo.readBuffer[socketInfo.BytesRECV - 1] == '\r')
	if (firstNewlinePos != bufferEnd)
	{
		if (socketInfo.readBuffer == firstNewlinePos)
		{
			_input->assign("\n");
		}
		else
		{
			_input->assign(socketInfo.readBuffer, firstNewlinePos);
		}
		const char newLineMarker = *firstNewlinePos;
		do // skips just ONE newline
		{
			++firstNewlinePos;
		} while ((firstNewlinePos != bufferEnd) && (std::find(NEWLINE_MARKERS, NEWLINE_MARKERS_END, *firstNewlinePos) != NEWLINE_MARKERS_END) && (newLineMarker != *firstNewlinePos));

		size_t newSize = bufferEnd - firstNewlinePos;
		socketInfo.BytesRECV -= (firstNewlinePos - socketInfo.readBuffer);
		assert(socketInfo.BytesRECV == newSize);
		if (newSize != 0)
		{
			memcpy(socketInfo.readBuffer, firstNewlinePos, newSize);
			socketInfo.bufferMaybeReady = true;
		}
		else
		{
			socketInfo.bufferMaybeReady = false;
		}
		//_input->erase(_input->find_last_not_of(" \n\r\t")+1);
		return true;
	}

	return false;
}

void AsyncNetworkHandler::PutOutput(size_t _socketid, const std::string &_output)
{
	/*if (_socketid >= sizeof(m_socketArray) / sizeof(*m_socketArray))
	{
		assert(_socketid < m_virtualSocketIdToRealSocketId.size());
		size_t virtualSocketId = _socketid;
		_socketid = m_virtualSocketIdToRealSocketId[virtualSocketId];

		char buf[30];
		_itoa_s(virtualSocketId, buf, 10);
		strcat_s(buf, ",");
		_itoa_s(_output.size(), &buf[strlen(buf)], 30 - strlen(buf), 10);
		strcat_s(buf, ",");
		size_t prefixLen = strlen(buf);
		memcpy(&m_socketArray[_socketid]->writeBuffer[m_socketArray[_socketid]->writeLen], buf, prefixLen);
		m_socketArray[_socketid]->writeLen += prefixLen;
	}*/

	assert(m_socketArray[_socketid]);
#ifdef WIN32
	for (int i = 0 ; i < _output.size() ; ++i)
	{
		if (_output[i] == '\n')
		{
			assert(m_socketArray[_socketid]->writeLen + 2 <= sizeof(m_socketArray[_socketid]->writeBuffer));
			m_socketArray[_socketid]->writeBuffer[m_socketArray[_socketid]->writeLen++] = '\r';
			m_socketArray[_socketid]->writeBuffer[m_socketArray[_socketid]->writeLen++] = '\n';
		}
		else
		{
			assert(m_socketArray[_socketid]->writeLen + 1 <= sizeof(m_socketArray[_socketid]->writeBuffer));
			m_socketArray[_socketid]->writeBuffer[m_socketArray[_socketid]->writeLen++] = _output[i];
		}
	}
#else
	memcpy(&m_socketArray[_socketid]->writeBuffer[m_socketArray[_socketid]->writeLen], _output.c_str(), _output.size());
	m_socketArray[_socketid]->writeLen += _output.size();
#endif
}

size_t AsyncNetworkHandler::CreateSocketInformation(SOCKET s)
{
 
   spdlog::info("Accepted socket number {}", s);
 
   std::unique_ptr<SOCKET_INFORMATION> SI = std::make_unique<SOCKET_INFORMATION>(s);

   size_t socketIdx = 0;
	for ( ; (socketIdx < FD_SETSIZE) && m_socketArray[socketIdx] ; ++socketIdx)
		;

	m_socketArray[socketIdx] = std::move(SI);

	m_newConnectionFunc(socketIdx);
	return socketIdx;
}
 
void AsyncNetworkHandler::FreeSocketInformation(DWORD a_index)
{
   std::unique_ptr<SOCKET_INFORMATION> SI = std::move(m_socketArray[a_index]);
	assert(!m_socketArray[a_index]);

   closesocket(SI->Socket);
   spdlog::info("Closing socket number {}", SI->Socket);

   m_closedConnectionFunc(a_index);
}

} // simplenet
