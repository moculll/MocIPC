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

template <typename T = miStdString>
static inline T stringToMiString(const std::string& str) {
	if constexpr (std::is_same_v<T, std::wstring>) {
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
		std::wstring wstr(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
		return wstr;
	}
	else {
		return str;
	}
}

} /* IPCDefines */

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

static inline IPCDefines::miStdString generateRandomStdString(int length) {
	IPCDefines::miStdString charset = miConstString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	IPCDefines::miStdString result;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<size_t> dis(0, charset.size() - 1);

	for (int i = 0; i < length; ++i) {
		result += charset[dis(gen)];
	}

	return result;
}

static inline std::string generateRandomCommonStdString(int length) {
	std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	std::string result;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<size_t> dis(0, charset.size() - 1);

	for (int i = 0; i < length; ++i) {
		result += charset[dis(gen)];
	}

	return result;
}

static inline std::wstring stringToWString(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
	return wstr;
}

static inline std::string wstringToString(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
	std::string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
	return str;
}

static inline HANDLE createPipe(const IPCDefines::miStdString &pipeName, uint32_t outBufferSize, uint32_t inBufferSize)
{
	return CreateNamedPipe(
		pipeName.c_str(),
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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

	IPCUnit() : recvHook(nullptr), connThread(), connMap(), connList() {}

	uint32_t write(const IPCDefines::miStdString& name, void* src, uint32_t size) {
		auto target = connMap.find(name);
		if (target == connMap.end())
			return 0;
		char buffer[4096];
		memcpy(buffer, src, size);
		std::future<bool> result = std::async(std::launch::async, [buffer, size, target] {
			ResetEvent(target->second.second.writeOverlapped.hEvent);
			bool bReadDone = false;
			while (!bReadDone) {
				bReadDone = WriteFile(target->second.first, buffer, size, NULL, &target->second.second.writeOverlapped);
				DWORD dwError = GetLastError();

				if (!bReadDone && (dwError == ERROR_IO_PENDING)) {

					WaitForSingleObject(target->second.second.writeOverlapped.hEvent, INFINITE);
					bReadDone = true;

				}
				else {
					continue;
				}
			}
			return true;
		});
		return 0;
	}
	uint32_t write(uint32_t index, void* src, uint32_t size) {
		if (index >= connList.size())
			return 0;
		auto target = connMap.find(connList[index].first);
		if(target == connMap.end())
			return 0;
		return this->write(target->first, src, size);
		
	}
	uint32_t write(void* src, uint32_t size) {
		return write(0, src, size);

	}
	void handleConnections() {
		

		while (1) {

			for (auto conn = connMap.begin(); conn != connMap.end(); ++conn) {
				char buffer[4096] = {0};
				ResetEvent(conn->second.second.readOverlapped.hEvent);
				bool bReadDone = false;
				while (!bReadDone) {
					bReadDone = ReadFile(conn->second.first, &buffer[4], sizeof(buffer) - 4, NULL, &conn->second.second.readOverlapped);
					DWORD dwError = GetLastError();

					if (!bReadDone && (dwError == ERROR_IO_PENDING)) {
						MOCIPC_DBGPRINT("server waiting to read client response");
						WaitForSingleObject(conn->second.second.readOverlapped.hEvent, INFINITE);
						memcpy(buffer, &conn->second.second.readOverlapped.InternalHigh, 4);
						
						bReadDone = true;
						break;
					}
					else {
						continue;
					}
				}

				if (recvHook) {
					recvHook(buffer);
					MOCIPC_DBGPRINT("server received %lld bytes", conn->second.second.readOverlapped.InternalHigh);
				}

				/*FlushFileBuffers(conn->second.first);*/

			}



		}



	}
	using recvHookType_t = std::function<void(void*)>;
	void registerRecvHOOK(recvHookType_t callback) {
		recvHook = callback;
	}
protected:
	
	struct overlapTable_t {
		OVERLAPPED readOverlapped;
		OVERLAPPED writeOverlapped;
		OVERLAPPED connectOverlapped;
		overlapTable_t()
		{
			readOverlapped = {};
			writeOverlapped = {};
			connectOverlapped = {};
			readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			writeOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			connectOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		}
		overlapTable_t(overlapTable_t&& src) noexcept : readOverlapped(std::move(src.readOverlapped)), writeOverlapped(std::move(src.writeOverlapped)), connectOverlapped(std::move(src.connectOverlapped)) {}
	};

	recvHookType_t recvHook;
	std::thread connThread;
	std::map<IPCDefines::miStdString, std::pair<HANDLE, overlapTable_t> > connMap;
	std::vector<std::pair<IPCDefines::miStdString, std::thread>> connList;

private:



	
	

};

class IPCServer final : public IPCUnit {
public:
	IPCServer() : IPCServer(IPCDefines::miStdString(miConstString("\\\\.\\pipe\\MOCIPCDeamon"))) {}
	IPCServer(const IPCDefines::miStdString &publicPipeName)
	{
		MOCIPC_DBGPRINT("server Inited!");
		connThread = std::thread([this, publicPipeName] {
			
			while (1) {
				overlapTable_t overlaps;
				HANDLE deamonHandle = IPCStaticLibrary::createDefaultPipe(publicPipeName);
				MOCIPC_DBGPRINT("IPC server deamon pipe create new!");
				if (deamonHandle == INVALID_HANDLE_VALUE) {
					MOCIPC_DBGPRINT("deamon handle is invalid");
					continue;
				}
					
				MOCIPC_DBGPRINT("IPC server deamon pipe created!");
				/* non block function */
				bool connected = ConnectNamedPipe(deamonHandle, &overlaps.connectOverlapped) == 0 ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
				if (!connected && GetLastError() != ERROR_IO_PENDING) {
					
					CloseHandle(deamonHandle);
					continue;
				}
					
				
				
				MOCIPC_DBGPRINT("Client connected!");
				
				
				MOCIPC_DBGPRINT("IPC server deamon found client connected, creating new random channel!");
				std::string newConnectionName = std::string("\\\\.\\pipe\\MOCIPC") + IPCStaticLibrary::generateRandomCommonStdString(8);

				IPCDefines::miStdString newConnectionNameToCreate = IPCDefines::stringToMiString(newConnectionName);

				HANDLE newConnectionHandle = IPCStaticLibrary::createDefaultPipe(newConnectionNameToCreate);
				DWORD bytesWritten = 0;
				DWORD bytesToWrite = static_cast<DWORD>(IPCStaticLibrary::getExactSizeofString(newConnectionName));
				bool bReadDone = false;
				while (!bReadDone) {
					bReadDone = WriteFile(deamonHandle, (void*)newConnectionName.c_str(), bytesToWrite, &bytesWritten, &overlaps.writeOverlapped);
					DWORD dwError = GetLastError();

					if (!bReadDone && (dwError == ERROR_IO_PENDING)) {
						MOCIPC_DBGPRINT("server waiting to write new name");
						WaitForSingleObject(overlaps.writeOverlapped.hEvent, INFINITE);
						bReadDone = true;

					}
					else {
						continue;
					}
				}
				
		

				MOCIPC_DBGPRINT("write new connection to client, public size: %d, written: %d", bytesToWrite, bytesWritten);

				DWORD bytesReadStringLen = 0;
				bReadDone = false;
				while (!bReadDone) {
					bReadDone = ReadFile(deamonHandle, &bytesReadStringLen, sizeof(bytesReadStringLen), NULL, &overlaps.readOverlapped);
					DWORD dwError = GetLastError();

					if (!bReadDone && (dwError == ERROR_IO_PENDING)) {
						MOCIPC_DBGPRINT("server waiting to read client response");
						WaitForSingleObject(overlaps.readOverlapped.hEvent, INFINITE);
						bReadDone = true;

					}
					else {
						continue;
					}
				}

				MOCIPC_DBGPRINT("read from public response, size: %lld, len: %d", overlaps.readOverlapped.InternalHigh, bytesReadStringLen);

				DisconnectNamedPipe(deamonHandle);
				CloseHandle(deamonHandle);

				MOCIPC_DBGPRINT("new connection handle: %llx", (uintptr_t)newConnectionHandle);

				ConnectNamedPipe(newConnectionHandle, NULL);
				bool ready = false;
				std::thread handleThread = std::thread([this,&ready] {
					while(!ready);
					handleConnections();
				});
				connMap.emplace(newConnectionNameToCreate, std::pair<HANDLE, overlapTable_t>(std::move(newConnectionHandle), std::move(overlaps)));
				connList.emplace_back(std::pair<IPCDefines::miStdString, std::thread>(newConnectionNameToCreate, std::move(handleThread)));
				ready = true;
			}

		});

	}

private:


}; /* class IPCServer */

class IPCClient final : public IPCUnit {
public:
	using MessageCallback = std::function<void(const IPCDefines::miStdString&)>;
	using recvHookType_t = std::function<void(void*)>;
	IPCDefines::miStdString publicPipeName;

	IPCClient() : IPCClient(MocIPC::IPCDefines::miStdString(miConstString("\\\\.\\pipe\\MOCIPCDeamon"))) {}
	IPCClient(const IPCDefines::miStdString &publicPipeName) : publicPipeName(publicPipeName) {
		MOCIPC_DBGPRINT("client Inited!");

		connThread = std::thread([this] {
			while (true) {
				overlapTable_t overlaps;

				HANDLE publicPipe = CreateFile(
					this->publicPipeName.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_EXISTING,
					FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
					NULL
				);

				if(publicPipe == INVALID_HANDLE_VALUE)
					continue;
				MOCIPC_DBGPRINT("client connect to public server");
				char buffer[4096];

				bool bReadDone = false;
				while (!bReadDone) {
					bReadDone = ReadFile(publicPipe, buffer, sizeof(buffer), NULL, &overlaps.readOverlapped);
					DWORD dwError = GetLastError();

					if (!bReadDone && (dwError == ERROR_IO_PENDING)) {
						MOCIPC_DBGPRINT("client waiting for new name");
						WaitForSingleObject(overlaps.readOverlapped.hEvent, INFINITE);
						bReadDone = true;

					}
					else {
						continue;
					}
				}

				MOCIPC_DBGPRINT("client read from server done, len: %lld", overlaps.readOverlapped.InternalHigh);

				bReadDone = false;
				while (!bReadDone) {
					bReadDone = WriteFile(publicPipe, &overlaps.readOverlapped.InternalHigh, sizeof(overlaps.readOverlapped.InternalHigh), NULL, &overlaps.writeOverlapped);
					DWORD dwError = GetLastError();

					if (!bReadDone && (dwError == ERROR_IO_PENDING)) {

						WaitForSingleObject(overlaps.writeOverlapped.hEvent, INFINITE);
						bReadDone = true;

					}
					else {
						continue;
					}
				}
				
				MOCIPC_DBGPRINT("client write to public size: %lld, origin: %lld", overlaps.writeOverlapped.InternalHigh, overlaps.readOverlapped.InternalHigh);
				CloseHandle(publicPipe);

				
				std::string newConnectionName(buffer, overlaps.readOverlapped.InternalHigh);


				MOCIPC_DBGPRINT("received name: %s from server", newConnectionName.c_str());
				

				IPCDefines::miStdString newConnectionNameToCreate = IPCDefines::stringToMiString(newConnectionName);
				HANDLE clientPipe = INVALID_HANDLE_VALUE;
				do {
					clientPipe = CreateFile(
						newConnectionNameToCreate.c_str(),
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
						NULL
					);

				} while(clientPipe == INVALID_HANDLE_VALUE);
				
				MOCIPC_DBGPRINT("client connected to new pipe %s, handle: %llx", newConnectionName.c_str(), (uintptr_t)clientPipe);



				std::thread handleThread = std::thread([this] {
					handleConnections();
				});
				connMap.emplace(newConnectionNameToCreate, std::move(std::pair<HANDLE, overlapTable_t>(std::move(clientPipe), std::move(overlaps))));
				connList.emplace_back(std::move(std::make_pair(newConnectionNameToCreate, std::move(handleThread))));


				MOCIPC_DBGPRINT("handling data received from server, pipe: %s", connMap.begin()->first.c_str());
				

				MOCIPC_DBGPRINT("client stop to receive data, close.");
				break;
			}
		});
	}

	



private:

}; /* class IPCClient */

} /* MocIPC */