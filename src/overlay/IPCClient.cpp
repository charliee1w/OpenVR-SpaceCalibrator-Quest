#include "stdafx.h"
#include "IPCClient.h"

#include <string>

std::string WStringToString(const std::wstring& wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string str_to(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str_to[0], size_needed, nullptr, nullptr);
	return str_to;
}

static std::string LastErrorString(DWORD lastError)
{
	LPWSTR buffer = nullptr;
	size_t size = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buffer, 0, nullptr
	);
	std::wstring message(buffer, size);
	LocalFree(buffer);
	return WStringToString(message);
}

IPCClient::~IPCClient()
{
	if (pipe && pipe != INVALID_HANDLE_VALUE)
		CloseHandle(pipe);
}

void IPCClient::Disconnect()
{
	if (pipe != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe);
		pipe = INVALID_HANDLE_VALUE;
	}
}

bool IPCClient::IsConnected() const
{
	return pipe != INVALID_HANDLE_VALUE;
}

void IPCClient::EnsureConnected()
{
	if (!IsConnected()) {
		Connect();
	}
}

void IPCClient::Connect()
{
	Disconnect();
	LPCTSTR pipeName = TEXT(OPENVR_SPACECALIBRATOR_PIPE_NAME);

	constexpr int kMaxAttempts = 30;
	for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
	{
		WaitNamedPipe(pipeName, 1000);
		pipe = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
		if (pipe != INVALID_HANDLE_VALUE)
			break;
		Sleep(500);
	}

	if (pipe == INVALID_HANDLE_VALUE)
	{
		throw std::runtime_error("Space Calibrator driver unavailable. Make sure SteamVR is running, and the Space Calibrator addon is enabled in SteamVR settings.");
	}

	DWORD mode = PIPE_READMODE_MESSAGE;
	if (!SetNamedPipeHandleState(pipe, &mode, 0, 0))
	{
		DWORD lastError = GetLastError();
		throw std::runtime_error("Couldn't set pipe mode. Error " + std::to_string(lastError) + ": " + LastErrorString(lastError));
	}

	auto response = SendBlocking(protocol::Request(protocol::RequestHandshake));
	if (response.type != protocol::ResponseHandshake || response.protocol.version != protocol::Version)
	{
		throw std::runtime_error(
			"Incorrect driver version installed, try reinstalling Space Calibrator. (Client: " +
			std::to_string(protocol::Version) +
			", Driver: " +
			std::to_string(response.protocol.version) +
			")"
		);
	}
}

protocol::Response IPCClient::SendBlocking(const protocol::Request &request)
{
	EnsureConnected();
	if (!SendOnce(request)) {
		Disconnect();
		Connect();
		if (!SendOnce(request)) {
			DWORD lastError = GetLastError();
			throw std::runtime_error("Error writing IPC request after reconnect. Error " + std::to_string(lastError) + ": " + LastErrorString(lastError));
		}
	}
	return Receive();
}

bool IPCClient::SendOnce(const protocol::Request &request)
{
	DWORD bytesWritten;
	BOOL success = WriteFile(pipe, &request, sizeof request, &bytesWritten, 0);
	return success && bytesWritten == sizeof request;
}

void IPCClient::Send(const protocol::Request &request)
{
	if (!SendOnce(request))
	{
		DWORD lastError = GetLastError();
		throw std::runtime_error("Error writing IPC request. Error " + std::to_string(lastError) + ": " + LastErrorString(lastError));
	}
}

protocol::Response IPCClient::Receive()
{
	protocol::Response response(protocol::ResponseInvalid);
	DWORD bytesRead;

	BOOL success = ReadFile(pipe, &response, sizeof response, &bytesRead, 0);
	if (!success)
	{
		DWORD lastError = GetLastError();
		if (lastError != ERROR_MORE_DATA)
		{
			throw std::runtime_error("Error reading IPC response. Error " + std::to_string(lastError) + ": " + LastErrorString(lastError));
		}
	}

	if (bytesRead != sizeof response)
	{
		throw std::runtime_error("Invalid IPC response. Error SIZE_MISMATCH, got size " + std::to_string(bytesRead));
	}

	return response;
}
