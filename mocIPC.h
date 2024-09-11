#pragma once
#include <windows.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include <future>
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
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL
	);
}

static inline HANDLE createDefaultPipe(const IPCDefines::miStdString& pipeName)
{
	return IPCStaticLibrary::createPipe(pipeName, 2048, 2048);
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

		serverInfo_t(std::pair<HANDLE, IPCDefines::miStdString> &info, bool&& isReady)
			: connInfo(info), ready(std::move(isReady)) {}
	};

private:
	


	
	

};

class IPCServer : public IPCUnit {
public:
	
	
	
	IPCServer()
	{
		connMgr = std::thread([this] {
			HANDLE daemonHandle = INVALID_HANDLE_VALUE;
			HANDLE newConnectionHandle = INVALID_HANDLE_VALUE;
			DWORD bytesWritten = 0;
			DWORD bytesToWrite = 0;
			while (1) {

				daemonHandle = IPCStaticLibrary::createDefaultPipe(IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDaemon"));
				if (INVALID_HANDLE_VALUE == daemonHandle) {
					MOCIPC_DBGPRINT("IPC server daemon pipe create failed!");
					break;
				}
				/* block function */
				if (!ConnectNamedPipe(daemonHandle, NULL)) {
					CloseHandle(daemonHandle);
					MOCIPC_DBGPRINT("IPC server daemon pip already created, trying to close...");
					continue;
				}
				IPCDefines::miStdString newConnectionName = IPCDefines::miStdString("\\\\.\\pipe\\MOCIPC") + IPCStaticLibrary::generateRandomStdString(8);
				newConnectionHandle = IPCStaticLibrary::createDefaultPipe(newConnectionName);
				bytesToWrite = static_cast<DWORD>(IPCStaticLibrary::getExactSizeofString(newConnectionName));
				if (!WriteFile(daemonHandle, (void*)newConnectionName.c_str(), bytesToWrite, &bytesWritten, NULL)) {
					MOCIPC_DBGPRINT("Failed to write to pipe. Error: %d", GetLastError());
					CloseHandle(newConnectionHandle);
					DisconnectNamedPipe(daemonHandle);
					CloseHandle(daemonHandle);
					continue;
				}
				std::atomic<bool> ready(false);
				std::async(std::launch::async, [this, &ready, newConnectionHandle] {
					ConnectNamedPipe(newConnectionHandle, NULL);
					ready.store(true, std::memory_order_relaxed);

					handleClientConnection(newConnectionHandle);
				});
				std::pair<HANDLE, IPCDefines::miStdString> connInfo = std::make_pair(newConnectionHandle, newConnectionName);
				serverInfo_t serverInfo(connInfo, std::move(ready));

				info.emplace(info.size(), std::move(serverInfo));

				DisconnectNamedPipe(daemonHandle);
				CloseHandle(daemonHandle);

			}

		});

	}
	void registerRecvHOOK(recvHookType_t callback) {
		recvHook = callback;
	}
private:
	std::thread connMgr;
	/* { id : info } */
	static std::map<uint32_t, serverInfo_t> info;

	recvHookType_t recvHook;
	void handleClientConnection(HANDLE newConnectionHandle) {
		char buffer[4096];
		DWORD bytesRead;
		while (ReadFile(newConnectionHandle, buffer, sizeof(buffer), &bytesRead, NULL)) {
			if (recvHook)
				recvHook(buffer);
			
		}
	}
	uint32_t write(int index, void* src, uint32_t size) {
		DWORD bytesWritten = 0;
		if(index > info.size() - 1)
			return bytesWritten;
		WriteFile(info[index].connInfo.first, src, size, &bytesWritten, NULL);
		return bytesWritten;
	}
};

class IPCClient : public IPCUnit {
public:
	using MessageCallback = std::function<void(const IPCDefines::miStdString&)>;
	using recvHookType_t = std::function<void(void*)>;

	IPCClient(const IPCDefines::miStdString& publicPipeName) : publicPipeName(publicPipeName) {
		 std::thread([this, publicPipeName] {
			while (true) {
				if (!WaitNamedPipe(publicPipeName.c_str(), NMPWAIT_WAIT_FOREVER)) {
					MOCIPC_DBGPRINT("Failed to wait for named pipe");
					continue;
				}

				HANDLE hPipe = CreateFile(
					publicPipeName.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					0,
					NULL
				);
				if (hPipe == INVALID_HANDLE_VALUE) {
					MOCIPC_DBGPRINT("Failed to connect to the daemon pipe. Error: %d", GetLastError());
					continue;
				}

				char buffer[4096];
				DWORD bytesRead;
				while (ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL)) {
					IPCDefines::miStdString newConnectionName(buffer, bytesRead);

					HANDLE clientPipe = CreateFile(
						newConnectionName.c_str(),
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						0,
						NULL
					);
					if (clientPipe != INVALID_HANDLE_VALUE) {
						std::lock_guard<std::mutex> lock(mutex);
						connections.emplace(clientPipe, newConnectionName);
						handleServerConnection(clientPipe);
						break;
					}
					else {
						MOCIPC_DBGPRINT("Failed to connect to client pipe. Error: %d", GetLastError());
					}
				}
				CloseHandle(hPipe);
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

private:
	IPCDefines::miStdString publicPipeName;
	std::map<HANDLE, IPCDefines::miStdString> connections;
	recvHookType_t recvHook;
	std::mutex mutex;

	void handleServerConnection(HANDLE clientPipe) {
		char buffer[4096];
		DWORD bytesRead;
		while (ReadFile(clientPipe, buffer, sizeof(buffer), &bytesRead, NULL)) {
			if (recvHook) {
				recvHook(buffer);
			}
		}
		std::lock_guard<std::mutex> lock(mutex);
		connections.erase(clientPipe);
		CloseHandle(clientPipe);
	}
};

} /* MocIPC */