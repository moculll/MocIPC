#include "mocIPC.h"
#include <iostream>
#include <stdio.h>
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
const wchar_t* IPCBase::MOCIPC_DEFAULT_SHAREDSVC = L"\\\\.\\pipe\\MOCSVC";
const wchar_t* IPCBase::MOCIPC_DEFAULT_SHAREDCVS = L"\\\\.\\pipe\\MOCCVS";

IPCBase::IPCBase(const wchar_t* _sharedSVCName, const wchar_t* _sharedCVSName)
{
    size_t svcSize = wcslen(_sharedSVCName) + 1;
    size_t cvsSize = wcslen(_sharedCVSName) + 1;
    this->recvHOOK = nullptr;
    try {
        this->sharedSVCName = new wchar_t[svcSize];
        this->sharedCVSName = new wchar_t[cvsSize];
        this->buffer = new char[1024];
    }
    catch (const std::bad_alloc& e) {
        MOCIPC_DBGPRINT("Memory allocation failed: %s, check your memory!", e.what());
        this->sharedSVCName = nullptr;
        this->sharedCVSName = nullptr;
        this->buffer = nullptr;
        return;
    }
    
    wcscpy_s(this->sharedSVCName, svcSize, _sharedSVCName);
    wcscpy_s(this->sharedCVSName, cvsSize, _sharedCVSName);
    memset((void*)this->buffer, 0, 1024);

    MOCIPC_DBGPRINT("create Instance successful!");

}

HANDLE IPCBase::createNewPipe(const wchar_t* pipeName)
{
    return CreateNamedPipe(
        pipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        1024,
        1024,
        NMPWAIT_USE_DEFAULT_WAIT,
        NULL
    );
}

} /* MocIPC IPCBase */


namespace MocIPC {

IPCServer::IPCServer(const wchar_t* sharedSVCName, const wchar_t* sharedCVSName) : IPCBase(sharedSVCName, sharedCVSName)
{
    this->recvThread = std::thread(&IPCServer::recvThreadCallback, this);
}
void IPCServer::recvThreadCallback(IPCServer* obj)
{
    HANDLE hPipe;
    DWORD bytesRead;
    while (true) {
        WaitNamedPipe(obj->sharedCVSName, NMPWAIT_WAIT_FOREVER);
        hPipe = CreateFile(
            obj->sharedCVSName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (ReadFile(hPipe, obj->buffer + sizeof(uint32_t), 1024 - sizeof(uint32_t), &bytesRead, NULL)) {
            if (obj->recvHOOK) {
                *(uint32_t*)obj->buffer = bytesRead;
                obj->recvHOOK((void*)obj->buffer);
            }
                
            MOCIPC_DBGPRINT("receive len: %d", bytesRead);
        }

        FlushFileBuffers(hPipe);
    }

}

void IPCServer::write(void* src, uint32_t size)
{
    std::async(std::launch::async, [this, src, size]{ this->writeImpl(src, size); } );
}

void IPCServer::writeImpl(void* src, uint32_t size)
{
    DWORD bytesWritten;
    HANDLE hPipe = createNewPipe(this->sharedSVCName);
    if (hPipe == INVALID_HANDLE_VALUE) {
        MOCIPC_DBGPRINT("Failed to connect to pipe. Error: %d", GetLastError());
        return;
    }
    ConnectNamedPipe(hPipe, NULL);

    memcpy((void*)buffer, src, size);
    if (!WriteFile(hPipe, (void*)buffer, size, &bytesWritten, NULL)) {
        MOCIPC_DBGPRINT("Failed to write to pipe. Error: %d", GetLastError());
    }
    DisconnectNamedPipe(hPipe);
    MOCIPC_DBGPRINT("write buf len: %d", bytesWritten);
}

} /* MocIPC server */


namespace MocIPC {

IPCClient::IPCClient(const wchar_t* sharedSVCName, const wchar_t* sharedCVSName) : IPCBase(sharedSVCName, sharedCVSName)
{
    this->recvThread = std::thread(&IPCClient::recvThreadCallback, this);
}

void IPCClient::recvThreadCallback(IPCClient* obj)
{
    HANDLE hPipe;
    MOCIPC_DBGPRINT("done statrListening");
    DWORD bytesRead;
    while (true) {
        WaitNamedPipe(obj->sharedSVCName, NMPWAIT_WAIT_FOREVER);
        hPipe = CreateFile(
            obj->sharedSVCName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (ReadFile(hPipe, obj->buffer + sizeof(uint32_t), 1024 - sizeof(uint32_t), &bytesRead, NULL)) {
            if (obj->recvHOOK) {
                *(uint32_t*)obj->buffer = bytesRead;
                obj->recvHOOK((void*)obj->buffer);
            }
            MOCIPC_DBGPRINT("receive len: %d", bytesRead);
        }

        FlushFileBuffers(hPipe);
    }

}

void IPCClient::write(void* src, uint32_t size)
{
    std::async(std::launch::async, [this, src, size] {this->writeImpl(src, size); });
}

void IPCClient::writeImpl(void* src, uint32_t size)
{
    DWORD bytesWritten;
    HANDLE hPipe = createNewPipe(this->sharedCVSName);
    if (hPipe == INVALID_HANDLE_VALUE) {
        MOCIPC_DBGPRINT("Failed to connect to pipe. Error: %d", GetLastError());
        return;
    }
    ConnectNamedPipe(hPipe, NULL);

    memcpy((void*)buffer, src, size);
    if (!WriteFile(hPipe, (void*)buffer, size, &bytesWritten, NULL)) {
        MOCIPC_DBGPRINT("Failed to write to pipe. Error: %d", GetLastError());
    }
    DisconnectNamedPipe(hPipe);
    MOCIPC_DBGPRINT("write buf len: %d", bytesWritten);
}

} /* MocIPC client */

