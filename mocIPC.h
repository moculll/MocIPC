#pragma once
#include <windows.h>
#include <thread>

namespace MocIPC {
const wchar_t *MOCIPC_DEFAULT_SHAREDSVC = L"\\\\.\\pipe\\MOCSVC";
const wchar_t *MOCIPC_DEFAULT_SHAREDCVS = L"\\\\.\\pipe\\MOCCVS";


template <typename T>
inline T *getArg(void *arg)
{
	if (!arg)
		return (T *)0;
	return ((T *)((char *)arg + sizeof(uint32_t)));
}

inline uint32_t getSize(void *arg)
{
	return *(uint32_t*)arg;
}

class IPCBase {
public:
	using recvHookCallbackType = void(*)(void*);

	virtual void write(void* src, uint32_t size) = 0;
	IPCBase(const wchar_t* _sharedSVCName, const wchar_t* _sharedCVSName);
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
	HANDLE createNewPipe(const wchar_t* pipeName);

protected:
	std::thread recvThread;
	recvHookCallbackType recvHOOK;
	wchar_t *sharedSVCName;
	wchar_t *sharedCVSName;
	char* buffer;

};

class IPCServer : public IPCBase {
public:
	IPCServer(const wchar_t* sharedSVCName, const wchar_t* sharedCVSName);
	IPCServer() : IPCServer(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}
	void write(void* src, uint32_t size) override;
	
private:
	static void recvThreadCallback(IPCServer* obj);
	void writeImpl(void* src, uint32_t size);
};

class IPCClient : public IPCBase {
public:
	IPCClient(const wchar_t* sharedSVCName, const wchar_t* sharedCVSName);
	IPCClient() : IPCClient(MOCIPC_DEFAULT_SHAREDSVC, MOCIPC_DEFAULT_SHAREDCVS) {}
	void write(void* src, uint32_t size) override;

private:
	static void recvThreadCallback(IPCClient* obj);
	void writeImpl(void* src, uint32_t size);
};

} /* MocIPC */