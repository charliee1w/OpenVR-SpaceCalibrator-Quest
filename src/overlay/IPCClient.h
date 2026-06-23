#pragma once

#include "Protocol.h"

class IPCClient
{
public:
	~IPCClient();

	void Connect();
	void Disconnect();
	bool IsConnected() const;
	void EnsureConnected();
	protocol::Response SendBlocking(const protocol::Request &request);

	void Send(const protocol::Request &request);
	protocol::Response Receive();

private:
	HANDLE pipe = INVALID_HANDLE_VALUE;
	bool SendOnce(const protocol::Request &request);
};