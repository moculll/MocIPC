#pragma once
#include <windows.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <random>


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
	
static inline HANDLE createDefaultPipe(const IPCDefines::miStdString &pipeName)
{
	return IPCStaticLibrary::createPipe(pipeName, 2048, 2048);
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

} /* IPCStaticLibrary */

class IPCUnit {
public:
	IPCUnit()
	{
		
	}

	HANDLE openChannel(const IPCDefines::miStdString &pipeName)
	{
		IPCStaticLibrary::createDefaultPipe(pipeName);
		return ;
	}
	void closeChannel(HANDLE handleToClose)
	{

	}
protected:
	/*std::map<HANDLE, CHARTYPE*> a;*/



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
					continue;
				}
				connMap.emplace(connMap.size(), std::make_pair(newConnectionHandle, newConnectionName));
				DisconnectNamedPipe(daemonHandle);
				CloseHandle(daemonHandle);

			}

		});

	}

private:
	std::thread connMgr;
	/* { id : { pipeHandle : pipeName } } */
	std::map< uint32_t, std::pair<HANDLE, IPCDefines::miStdString> >connMap;
};

class IPCClient : public IPCUnit {
public:
	IPCClient()
	{
		connMgr = std::thread([this] {
			HANDLE hPipe = INVALID_HANDLE_VALUE;
			do {
				if(!WaitNamedPipe("\\\\.\\pipe\\MOCIPCDaemon", NMPWAIT_WAIT_FOREVER))
					continue;
				hPipe = CreateFile(
					"\\\\.\\pipe\\MOCIPCDaemon",
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					0,
					NULL
				);


			} while(1);
			
		});
	}
private:
	std::thread connMgr;
};

} /* MocIPC */