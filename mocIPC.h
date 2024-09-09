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



namespace IPCStaticLibrary {

	
static inline HANDLE createDefaultPipe(const CHARTYPE* pipeName)
{
	return IPCStaticLibrary::createPipe(pipeName, 2048, 2048);
}

static inline HANDLE createPipe(const CHARTYPE* pipeName, uint32_t outBufferSize, uint32_t inBufferSize)
{
	return CreateNamedPipe(
		pipeName,
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

	HANDLE openChannel(const CHARTYPE* pipeName)
	{
		IPCStaticLibrary::createDefaultPipe(pipeName);
		return 
	}
	void closeChannel(HANDLE handleToClose)
	{

	}
protected:
	std::map<HANDLE, CHARTYPE*> 
	HANDLE 


private:
	
	std::thread


	
	

};

class IPCServer : public IPCUnit {
public:


private:
	
	
};

class IPCClient : public IPCUnit {
public:
	

private:

};

} /* MocIPC */