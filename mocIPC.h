#pragma once
#include <windows.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#if UNICODE
#define CHARTYPE wchar_t
#define STRINGLEN wcslen
#define STRCPYSAFE wcscpy_s
#define STRSTR wcsstr
#else
#define CHARTYPE char
#define STRINGLEN strlen
#define STRCPYSAFE strcpy_s
#define STRSTR strstr
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

struct exchageMsg_t {
	union {
		char serverToClient[256];
		char clientToServer[256];
	};
};



class IPCBase {
public:
	static constexpr int BUFFER_BLOCK_SIZE = 1024;

	using recvHookCallbackType = void(*)(void*);
	static const CHARTYPE* MOCIPC_DEFAULT_SHAREDSVC;
	static const CHARTYPE* MOCIPC_DEFAULT_SHAREDCVS;
	static const CHARTYPE* MOCIPC_DEFAULT_SHAREDPREFIX;

	virtual int write(void* src, uint32_t size);
	virtual void writeImpl(void *src, uint32_t size) = 0;
	IPCBase(const CHARTYPE* _sharedSVCName, const CHARTYPE* _sharedCVSName);
	IPCBase() : IPCBase(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}
	~IPCBase()
	{

		delete sharedSVCName;
		delete sharedCVSName;

		sharedSVCName = nullptr;
		sharedCVSName = nullptr;
		recvHOOK = nullptr;
	}
	void registerRecvHOOK(recvHookCallbackType cb)
	{
		recvHOOK = cb;
	}
	
	HANDLE createNewPipe(const CHARTYPE* pipeNameSimple);
protected:
	HANDLE createNewPipeInternal(const CHARTYPE* pipeNameComplete);

	std::thread recvThread;
	recvHookCallbackType recvHOOK;
	CHARTYPE* sharedSVCName;
	CHARTYPE* sharedCVSName;

	struct controlBlock_t {
		HANDLE publicPipe;
		struct info_t {
			HANDLE pipe;
			CHARTYPE pipeName[256];
			CHARTYPE buffer[BUFFER_BLOCK_SIZE];
			info_t(HANDLE p, const CHARTYPE *name) : pipe(p) {
				memcpy(pipeName, name, 256);
				memset(buffer, 0, BUFFER_BLOCK_SIZE);
			}
		};
		std::vector<info_t> infos;
	};
	

};

class IPCServer : public IPCBase {
public:
	IPCServer(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName);
	IPCServer() : IPCServer(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}

private:
	std::thread listenThread;
	void IPCServerListenConnectThreadCallback();
	void recvThreadCallback();
	void writeImpl(void* src, uint32_t size);

	const char *IPCServerAllocNewHandle();

	controlBlock_t scb;
	
};

class IPCClient : public IPCBase {
public:
	IPCClient(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName);
	IPCClient() : IPCClient(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}

private:
	void recvThreadCallback();
	void writeImpl(void* src, uint32_t size);
	controlBlock_t ccb;

};

} /* MocIPC */