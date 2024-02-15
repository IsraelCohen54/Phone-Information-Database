#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <thread>
#include "BlockingQueueMT.h"

namespace simplenet
{

class AsyncNetworkHandler;

class SimpleNetMT {
private:
	using SocketId = size_t;
	//class ConnectionImpl;
	using InputsQ = BlockingQueueMT<std::string>;
	using OutputsQ = BlockingQueueMT<std::pair<SocketId, std::string>>;
	using SharedOutputsQ = std::shared_ptr<OutputsQ>;
	using SharedInputsQ = std::shared_ptr<InputsQ>;

public:
	struct ConnectionClosed : std::exception
	{
		virtual const char *what() const override { return "connection closed"; }
	};

	class Connection
	{
	public:
		//Connection();
		Connection(SocketId a_socket, const SharedInputsQ &a_inputs, const SharedOutputsQ &a_outputsQ);
		Connection(const Connection& a_other) = delete;
		Connection(Connection&& a_other);
		Connection& operator=(Connection&& a_other);
		~Connection() = default;

		std::string ReadLine();
		void Write(const std::string &a_str);

	private:
		//std::unique_ptr<ConnectionImpl> m_pImpl;
		//SimpleNetMT &a_netHandler;
		SocketId m_socketId;
		SharedInputsQ m_inputsQ;
		SharedOutputsQ m_outputsQ;
	};

	SimpleNetMT(unsigned short a_port = 4000);
	~SimpleNetMT();
	SimpleNetMT(const SimpleNetMT& a_other) = delete;
	SimpleNetMT& operator=(const SimpleNetMT& a_other) = delete;

	Connection WaitForConnection();
	void Shutdown();

private:
	using ConnectionsQValue = std::pair<SocketId, SharedInputsQ>;
	using ConnectionsInfo = std::unordered_map<SocketId, SharedInputsQ>;

	/*class ConnectionImpl
	{
	public:
		std::string Read();
		std::string ReadLine();
		void Write(const std::string &a_str);

	private:
		SimpleNetMT &a_netHandler;
		SocketId m_socketId;
		InputsQ m_inputsQ;
	};*/
	static const OutputsQ::value_type &GetOutputsPoisionedApple();
	void Pump();
	void AddConnection(SocketId a_socketId);
	void RemoveConnection(SocketId a_socketId);

	std::unique_ptr<AsyncNetworkHandler> m_pImpl;
	BlockingQueueMT<ConnectionsQValue> m_connectionsQ;
	SharedOutputsQ m_outputsQ;
	ConnectionsInfo m_openConnections; 
	std::thread m_runner;
};

} // simplenet
