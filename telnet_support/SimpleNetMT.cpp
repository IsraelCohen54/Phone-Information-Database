#include <cassert>
#include "SimpleNetMT.h"
#include "AsyncNetworkHandler.h"

namespace simplenet
{

static const std::string INPUT_POISENED_APPLE{};

SimpleNetMT::SimpleNetMT(unsigned short a_port)
	: m_pImpl(
		std::make_unique<AsyncNetworkHandler>(
			a_port, 
			[this](SocketId a_id) { this->AddConnection(a_id); }, 
			[this](SocketId a_id) { this->RemoveConnection(a_id); } 
		))
	, m_connectionsQ{}
	, m_outputsQ(std::make_shared<OutputsQ>())
	, m_openConnections{}
	, m_runner{&SimpleNetMT::Pump, this}
{
}

SimpleNetMT::~SimpleNetMT()
{
	Shutdown();
	m_runner.join();
}

SimpleNetMT::Connection SimpleNetMT::WaitForConnection()
{
	ConnectionsQValue connectionInfo;
	m_connectionsQ.Dequeue(connectionInfo);

	return Connection(connectionInfo.first, connectionInfo.second, m_outputsQ);
}

void SimpleNetMT::Shutdown()
{
	m_outputsQ->Enqueue(GetOutputsPoisionedApple());
}

const SimpleNetMT::OutputsQ::value_type &SimpleNetMT::GetOutputsPoisionedApple()
{
	static OutputsQ::value_type s_poisioned{0, std::string{}};
	return s_poisioned;
}

void SimpleNetMT::Pump()
{
	SocketId sockId;
	std::string input;

	while (true)
	{
		bool hasInput = m_pImpl->GetInput(&sockId, &input, 10);
		if (hasInput && !input.empty())
		{
			ConnectionsInfo::iterator toNotify = m_openConnections.find(sockId);
			assert(toNotify != m_openConnections.end());
			toNotify->second->Enqueue(std::move(input));
		}

		OutputsQ::value_type output;
		while (m_outputsQ->TryDequeue(output))
		{
			if (output == GetOutputsPoisionedApple())
			{
				return;
			}
			if (m_openConnections.find(output.first) != m_openConnections.end())
			{
				m_pImpl->PutOutput(output.first, output.second);
			}
		}
	}
}

void SimpleNetMT::AddConnection(SocketId a_socketId)
{
	//assert(m_openConnections.find(a_socketId) == m_openConnections.end());
	SharedInputsQ q = std::make_shared<InputsQ>();
	bool isInsert = m_openConnections.insert(std::make_pair(a_socketId, q)).second;;
	assert(isInsert);
	m_connectionsQ.Enqueue(std::make_pair(a_socketId, std::move(q)));
}

void SimpleNetMT::RemoveConnection(SocketId a_socketId)
{
	ConnectionsInfo::iterator toRemove = m_openConnections.find(a_socketId);
	assert(toRemove != m_openConnections.end());
	toRemove->second->Enqueue(INPUT_POISENED_APPLE);
	m_openConnections.erase(toRemove);
}

/*SimpleNetMT::Connection::Connection()
	: m_leftovers{}
	, m_socketId(-1)
	, m_inputsQ()
	, m_outputsQ()
{
}*/

SimpleNetMT::Connection::Connection(SocketId a_socket, const SharedInputsQ &a_inputsQ, const SharedOutputsQ &a_outputsQ)
	: m_socketId(a_socket)
	, m_inputsQ(a_inputsQ)
	, m_outputsQ(a_outputsQ)
{
}

SimpleNetMT::Connection::Connection(Connection&& a_other)
	: m_socketId(a_other.m_socketId)
	, m_inputsQ(std::move(a_other.m_inputsQ))
	, m_outputsQ(std::move(a_other.m_outputsQ))
{
}

SimpleNetMT::Connection& SimpleNetMT::Connection::operator=(Connection&& a_other)
{
	m_socketId = a_other.m_socketId;
	m_inputsQ = std::move(a_other.m_inputsQ);
	m_outputsQ = std::move(a_other.m_outputsQ);
	return *this;
}

std::string SimpleNetMT::Connection::ReadLine()
{
	std::string popped;
	m_inputsQ->Dequeue(popped);
	if (popped == INPUT_POISENED_APPLE)
	{
		throw ConnectionClosed();
	}
	return std::move(popped);
}

void SimpleNetMT::Connection::Write(const std::string &a_str)
{
	assert(!a_str.empty() && "outputing empty strings is not allowed");
	m_outputsQ->Enqueue(std::make_pair(m_socketId, a_str));
}

/*		std::string m_leftovers;
		SocketId m_socketId;
		SharedInputsQ m_inputsQ;
		SharedOutputsQ &m_outputsQ;
		*/

} // simplenet
