#pragma once
#include <windows.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include <future>
#include <utility>
#include <cstdint>
#define MOCIPC_DBGPRINT_ENABLE 1
#if MOCIPC_DBGPRINT_ENABLE

#define MOCIPC_DBGPRINT(fmt, ...) \
            do { \
                printf(fmt " [%s]" "\n", ##__VA_ARGS__, __func__); \
            } while(0)

#else
#define MOCIPC_DBGPRINT(fmt, ...)
#endif
namespace MocIPC {

template <typename T>
inline typename std::enable_if<std::is_pointer<T>::value, T>::type getArg(void* arg) {
	if (!arg) return nullptr;
	return reinterpret_cast<T>(reinterpret_cast<char*>(arg) + sizeof(uint32_t));
}

template <typename T>
inline typename std::enable_if<!std::is_pointer<T>::value, T>::type getArg(void* arg) {
	if (!arg) return T{0};
	return *reinterpret_cast<T*>(reinterpret_cast<char*>(arg) + sizeof(uint32_t));
}

inline uint32_t getSize(void* arg)
{
	return (arg ? *(uint32_t*)arg : 0);
}

namespace IPCDefines {
#if UNICODE
using miCharType_t = wchar_t;
using miStdString = std::wstring;
#define miStrlen wcslen
#define miStrcpy wcscpy_s
#define miStrstr wcsstr
#define miConstString(x) L##x
#else
using miCharType_t = char;
using miStdString = std::string;
#define miStrlen strlen
#define miStrcpy strcpy_s
#define miStrstr strstr
#define miConstString(x) x
#endif

}

namespace IPCStaticLibrary {

template <typename T>
std::size_t getExactSizeofString(const T& str) {
	if constexpr (std::is_same_v<T, std::string>) {
		return str.size() + 1;
	}
	else if constexpr (std::is_same_v<T, std::wstring>) {
		return (str.size() + 1) * sizeof(wchar_t);
	}
	else {
		static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>,
			"Unsupported string type");
		return 0;
	}
}

IPCDefines::miStdString generateRandomStdString(int length) {
	IPCDefines::miStdString charset = miConstString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	IPCDefines::miStdString result;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, charset.size() - 1);

	for (int i = 0; i < length; ++i) {
		result += charset[dis(gen)];
	}

	return result;
}
	
static inline HANDLE createPipe(const IPCDefines::miStdString &pipeName, uint32_t outBufferSize, uint32_t inBufferSize)
{
	return CreateNamedPipe(
		pipeName.c_str(),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		outBufferSize,
		inBufferSize,
		NMPWAIT_WAIT_FOREVER,
		NULL
	);
}

static inline HANDLE createDefaultPipe(const IPCDefines::miStdString& pipeName)
{
	return IPCStaticLibrary::createPipe(pipeName, 8192, 8192);
}

} /* IPCStaticLibrary */

class IPCUnit {
public:

	IPCUnit()
	{
		
	}

	
	
protected:
	/*std::map<HANDLE, CHARTYPE*> a;*/
	using recvHookType_t = std::function<void(void*)>;
	struct serverInfo_t {
		std::pair<HANDLE, IPCDefines::miStdString> connInfo;
		std::atomic<bool> ready;
		serverInfo_t& operator = (serverInfo_t&& src) noexcept
		{
			if (this != &src) {
				connInfo = std::move(src.connInfo);
				ready.store(src.ready.load());
				src.ready.store(false);
			}
		}
		serverInfo_t(serverInfo_t&& src) noexcept
			: connInfo(std::move(src.connInfo)), ready(src.ready.load()) {
			src.ready.store(false);
		}
		serverInfo_t() : connInfo(), ready(0) {}
		serverInfo_t(std::pair<HANDLE, IPCDefines::miStdString> &&info, std::atomic<bool> &isReady)
			: connInfo(std::move(info)) {
			ready.store(isReady.load());
		}
	};

private:
	


	
	

};

class IPCServer : public IPCUnit {
public:
	
	
	
	IPCServer() : connMgr(), info(), recvHook(nullptr)
	{
		MOCIPC_DBGPRINT("server Inited!");
		MOCIPC_DBGPRINT("infoSize: %lld", info.size());
		connMgr = std::thread([this] {
			
			HANDLE newConnectionHandle = INVALID_HANDLE_VALUE;
			
			/*daemonHandle = IPCStaticLibrary::createDefaultPipe(IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDaemon"));*/
			while (1) {
				HANDLE daemonHandle = IPCStaticLibrary::createDefaultPipe(IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDaemon"));
				MOCIPC_DBGPRINT("IPC server daemon pipe create new!");
				if(daemonHandle == INVALID_HANDLE_VALUE)
					continue;
				/*MOCIPC_DBGPRINT("IPC server daemon pipe created!");*/
				/* block function */
				if (!ConnectNamedPipe(daemonHandle, NULL)) {
					/*MOCIPC_DBGPRINT("IPC server daemon pip already created, trying to close...");*/
					continue;
				}
				MOCIPC_DBGPRINT("IPC server daemon found client connected, creating new random channel!");
				IPCDefines::miStdString newConnectionName = IPCDefines::miStdString("\\\\.\\pipe\\MOCIPC") + IPCStaticLibrary::generateRandomStdString(8);

				newConnectionHandle = IPCStaticLibrary::createDefaultPipe(newConnectionName);
				DWORD bytesWritten = 0;
				DWORD bytesToWrite = static_cast<DWORD>(IPCStaticLibrary::getExactSizeofString(newConnectionName));
				WriteFile(daemonHandle, (void*)newConnectionName.c_str(), bytesToWrite, &bytesWritten, NULL);
				if(bytesWritten <= bytesToWrite)
					continue;
				/* 3 times max retries */
				for (int i = 0; i < 3; i++) {
					DWORD bytesRead;
					DWORD bytesReadStringLen;
					bool result = ReadFile(daemonHandle, &bytesReadStringLen, sizeof(bytesReadStringLen), &bytesRead, NULL);
					if (bytesReadStringLen != bytesToWrite || !result) {
						MOCIPC_DBGPRINT("Failed, read from client stringlen: %d", bytesReadStringLen);
						break;
					}
					std::this_thread::sleep_for(std::chrono::microseconds(500));
				}
				DisconnectNamedPipe(daemonHandle);
				CloseHandle(daemonHandle);
				MOCIPC_DBGPRINT("new connection handle: %llx", (uintptr_t)newConnectionHandle);

				for (int i = 0; i < 3; i++) {
					if (!ConnectNamedPipe(newConnectionHandle, NULL)) {
						MOCIPC_DBGPRINT("failed to connect to client, exit");
						DWORD error = GetLastError();
						MOCIPC_DBGPRINT("ConnectNamedPipe failed with error: %lu", error);
						std::this_thread::sleep_for(std::chrono::microseconds(500));
					}
					break;
				}

				
				MOCIPC_DBGPRINT("writed name: %s to client!", newConnectionName.c_str());
				std::atomic<bool> ready(false);
				std::pair<HANDLE, IPCDefines::miStdString> connInfo = std::make_pair(newConnectionHandle, newConnectionName);
				serverInfo_t serverInfo(std::move(connInfo), ready);
				MOCIPC_DBGPRINT("infoSize: %lld", info.size());
				info.emplace(info.size(), std::move(serverInfo));
				MOCIPC_DBGPRINT("infoSize: %lld, new connection handle: %llx", info.size(), (uintptr_t)info.begin()->second.connInfo.first);
				std::async(std::launch::async, [this] {
					/*ready.store(true, std::memory_order_relaxed);*/
					MOCIPC_DBGPRINT("handling data received from client");
					handleClientConnection(info.begin()->second.connInfo.first);
				});

			}

		});

	}
	void registerRecvHOOK(recvHookType_t callback) {
		recvHook = callback;
	}
	uint32_t write(int index, void* src, uint32_t size) {
		DWORD bytesWritten = 0;
		if(!info.size())
			return bytesWritten;
		if (index > info.size()) {
			MOCIPC_DBGPRINT("server write index is bigger than we have");
			return bytesWritten;
		}
		MOCIPC_DBGPRINT("wtire to handle: %llx", (uintptr_t)info[index].connInfo.first);
		WriteFile(info[index].connInfo.first, src, size, &bytesWritten, NULL);
		return bytesWritten;
	}
private:
	std::thread connMgr;
	/* { id : info } */
	std::map<uint32_t, serverInfo_t> info;

	recvHookType_t recvHook;
	void handleClientConnection(HANDLE newConnectionHandle) {
		char buffer[4096];
		DWORD bytesRead;
		MOCIPC_DBGPRINT("server start recv, handle: %llx", (uintptr_t)newConnectionHandle);
		while (1) {
			bool ret = ReadFile(newConnectionHandle, buffer, sizeof(buffer), &bytesRead, NULL);
			if(bytesRead <= 0 || !ret) {
				MOCIPC_DBGPRINT("server read file failed");
				continue;
			}
				
			if (recvHook)
				recvHook(buffer);
		}
		MOCIPC_DBGPRINT("return from read");
	}
	
};

class IPCClient : public IPCUnit {
public:
	using MessageCallback = std::function<void(const IPCDefines::miStdString&)>;
	using recvHookType_t = std::function<void(void*)>;
	IPCDefines::miStdString publicPipeName;
	IPCClient(const IPCDefines::miStdString publicPipeName) : publicPipeName(publicPipeName) {
		MOCIPC_DBGPRINT("client Inited!");
		initThread = std::thread([this] {
			while (true) {
				if (!WaitNamedPipe(this->publicPipeName.c_str(), NMPWAIT_WAIT_FOREVER)) {
					MOCIPC_DBGPRINT("Failed to wait for named pipe");
					continue;
				}

				HANDLE publicPipe = CreateFile(
					this->publicPipeName.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					0,
					NULL
				);
				if (publicPipe == INVALID_HANDLE_VALUE) {
					MOCIPC_DBGPRINT("Failed to connect to the daemon pipe. Error: %d", GetLastError());
					continue;
				}

				IPCDefines::miCharType_t buffer[4096];
				DWORD bytesRead;
				DWORD bytedSendBackRead;
				for (int i = 0; i < 3; i++) {
					if (ReadFile(publicPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
						break;
					}
					std::this_thread::sleep_for(std::chrono::microseconds(500));
				}
				for (int i = 0; i < 3; i++) {
					WriteFile(publicPipe, &bytesRead, sizeof(bytesRead), &bytedSendBackRead, NULL);
					if (bytedSendBackRead && bytedSendBackRead == bytesRead)
						break;
					std::this_thread::sleep_for(std::chrono::microseconds(500));
				}
				
				
				DisconnectNamedPipe(publicPipe);
				CloseHandle(publicPipe);
				
				IPCDefines::miStdString newConnectionName(buffer, bytesRead / sizeof(IPCDefines::miCharType_t));
				MOCIPC_DBGPRINT("received name: %s from server", newConnectionName.c_str());
				


				if (!WaitNamedPipe(newConnectionName.c_str(), NMPWAIT_WAIT_FOREVER)) {
					MOCIPC_DBGPRINT("didn't catch new connection from server, wait for 1s...");
					/*std::this_thread::sleep_for(std::chrono::microseconds(1000))*/;
					return;
				}

				HANDLE clientPipe = INVALID_HANDLE_VALUE;
				while (clientPipe == INVALID_HANDLE_VALUE) {
					clientPipe = CreateFile(
						newConnectionName.c_str(),
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						0,
						NULL
					);
				}
				MOCIPC_DBGPRINT("client connected to new pipe %s, handle: %llx", newConnectionName.c_str(), (uintptr_t)clientPipe);
				if (clientPipe != INVALID_HANDLE_VALUE) {

					connections.emplace(std::move(clientPipe), std::move(newConnectionName));
					MOCIPC_DBGPRINT("handling data received from server, pipe: %llx", (uintptr_t)connections.begin()->first);

					handleServerConnection();
					break;
				}
				else {

					MOCIPC_DBGPRINT("Failed to connect to client pipe. Error: %d", GetLastError());
				}
				
				MOCIPC_DBGPRINT("client stop to receive data, close.");
				
			}
		});
	}

	void registerRecvHOOK(recvHookType_t callback) {
		recvHook = callback;
	}

	void sendMessage(const IPCDefines::miStdString& message, HANDLE pipeHandle) {
		if (pipeHandle != INVALID_HANDLE_VALUE) {
			DWORD bytesWritten;
			WriteFile(pipeHandle, message.c_str(), static_cast<DWORD>(message.size()), &bytesWritten, NULL);
		}
	}

	uint32_t write(HANDLE pipeHandle, void* src, uint32_t size) {
		DWORD bytesWritten = 0;
		if (pipeHandle != INVALID_HANDLE_VALUE) {
			WriteFile(pipeHandle, src, size, &bytesWritten, NULL);
		}
		return bytesWritten;
	}
	uint32_t write(void* src, uint32_t size) {
		DWORD bytesWritten = 0;
		MOCIPC_DBGPRINT("client connections size: %lld", connections.size());
		if (connections.size() && connections.begin()->first != INVALID_HANDLE_VALUE) {
			MOCIPC_DBGPRINT("client write data, handle: %llx", (uintptr_t)connections.begin()->first);
			WriteFile(connections.begin()->first, src, size, &bytesWritten, NULL);
		}
		return bytesWritten;
	}

private:
	
	std::map<HANDLE, IPCDefines::miStdString> connections;
	recvHookType_t recvHook;
	std::thread initThread;
	void handleServerConnection() {
		char buffer[4096];
		DWORD bytesRead;

		while (1) {
			if (!ReadFile(connections.begin()->first, buffer, sizeof(buffer), &bytesRead, NULL)) {
				
				continue;
			}
				
			if (recvHook) {
				recvHook(buffer);

			}

		}
		CloseHandle(connections.begin()->first);
		connections.erase(connections.begin());

		
	}
};

} /* MocIPC */