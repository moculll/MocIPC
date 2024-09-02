#pragma once
#include <windows.h>
#include <thread>
#include <map>
#if UNICODE
#define CHARTYPE wchar_t
#define STRINGLEN wcslen
#define STRCPYSAFE wcscpy_s
#else
#define CHARTYPE char
#define STRINGLEN strlen
#define STRCPYSAFE strcpy_s
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
		delete buffer;
		delete sharedSVCName;
		delete sharedCVSName;
		buffer = nullptr;
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
	char* buffer;
	std::map<int, int> handle;
};

class IPCServer : public IPCBase {
public:
	IPCServer(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName);
	IPCServer() : IPCServer(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}

private:
	static void recvThreadCallback(IPCServer* obj);
	void writeImpl(void* src, uint32_t size);
};

class IPCClient : public IPCBase {
public:
	IPCClient(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName);
	IPCClient() : IPCClient(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}

private:
	static void recvThreadCallback(IPCClient* obj);
	void writeImpl(void* src, uint32_t size);
};

} /* MocIPC */