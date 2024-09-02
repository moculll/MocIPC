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
#if UNICODE
const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDSVC = L"\\\\.\\pipe\\MOCSVC";
const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDCVS = L"\\\\.\\pipe\\MOCCVS";

const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDPREFIX = L"\\\\.\\pipe\\";
#else
const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDSVC = "\\\\.\\pipe\\MOCSVC";
const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDCVS = "\\\\.\\pipe\\MOCCVS";

const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDPREFIX = "\\\\.\\pipe\\";
#endif

IPCBase::IPCBase(const CHARTYPE* _sharedSVCName, const CHARTYPE* _sharedCVSName)
{

    size_t svcSize = STRINGLEN(_sharedSVCName) + 1;
    size_t cvsSize = STRINGLEN(_sharedCVSName) + 1;

    this->recvHOOK = nullptr;
    this->handle.emplace(0, 0);
    try {
        this->sharedSVCName = new CHARTYPE[svcSize];
        this->sharedCVSName = new CHARTYPE[cvsSize];
        this->buffer = new char[BUFFER_BLOCK_SIZE];
    }
    catch (const std::bad_alloc& e) {
        MOCIPC_DBGPRINT("Memory allocation failed: %s, check your memory!", e.what());
        this->sharedSVCName = nullptr;
        this->sharedCVSName = nullptr;
        this->buffer = nullptr;
        return;
    }

    STRCPYSAFE(this->sharedSVCName, svcSize, _sharedSVCName);
    STRCPYSAFE(this->sharedCVSName, cvsSize, _sharedCVSName);

    

    MOCIPC_DBGPRINT("create Instance successful!");

}

/**
 * @brief according to documention, ReadFile/WriteFile/ConnectNamedPipe will return after 50ms
 */
HANDLE IPCBase::createNewPipeInternal(const CHARTYPE* pipeNameComplete)
{
    return CreateNamedPipe(
        pipeNameComplete,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        BUFFER_BLOCK_SIZE,
        BUFFER_BLOCK_SIZE,
        NMPWAIT_USE_DEFAULT_WAIT,
        NULL
    );
}

HANDLE IPCBase::createNewPipe(const CHARTYPE* pipeNameSimple)
{
    /* according to MicroSoft's defination */
    CHARTYPE tmp[256];
#if UNICODE
    swprintf_s(tmp, L"%s%s", MOCIPC_DEFAULT_SHAREDPREFIX, pipeNameSimple);
#else
    sprintf_s(tmp, "%s%s", MOCIPC_DEFAULT_SHAREDPREFIX, pipeNameSimple);
#endif
    return createNewPipeInternal(tmp);
}


int IPCBase::write(void* src, uint32_t size)
{
    
    auto future = std::async(std::launch::async, [this, src, size] {
        this->writeImpl(src, size);
    });
    

    return 0;
}


} /* MocIPC IPCBase */


namespace MocIPC {

IPCServer::IPCServer(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName) : IPCBase(sharedSVCName, sharedCVSName)
{
    this->recvThread = std::thread(&IPCServer::recvThreadCallback, this);
}
void IPCServer::recvThreadCallback(IPCServer* obj)
{
    HANDLE hPipe;
    DWORD bytesRead;
    while (true) {
        memset(obj->buffer, 0, BUFFER_BLOCK_SIZE);
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
        if (ReadFile(hPipe, obj->buffer + sizeof(uint32_t), BUFFER_BLOCK_SIZE - sizeof(uint32_t), &bytesRead, NULL)) {
            if (obj->recvHOOK) {
                *(uint32_t*)obj->buffer = bytesRead;
                obj->recvHOOK((void*)obj->buffer);
            }

            MOCIPC_DBGPRINT("[%s] receive len: %d", obj->sharedCVSName, bytesRead);
        }

        FlushFileBuffers(hPipe);
        CloseHandle(hPipe);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

}


void IPCServer::writeImpl(void* src, uint32_t size)
{
    DWORD bytesWritten;
    HANDLE hPipe = createNewPipeInternal(this->sharedSVCName);
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
    CloseHandle(hPipe);
    MOCIPC_DBGPRINT("[%s] write buf len: %d", this->sharedSVCName, bytesWritten);
}

} /* MocIPC server */


namespace MocIPC {

IPCClient::IPCClient(const CHARTYPE* sharedSVCName, const CHARTYPE* sharedCVSName) : IPCBase(sharedSVCName, sharedCVSName)
{
    this->recvThread = std::thread(&IPCClient::recvThreadCallback, this);
}

void IPCClient::recvThreadCallback(IPCClient* obj)
{
    HANDLE hPipe;
    DWORD bytesRead;
    while (true) {
        memset(obj->buffer, 0, BUFFER_BLOCK_SIZE);
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
        if (ReadFile(hPipe, obj->buffer + sizeof(uint32_t), BUFFER_BLOCK_SIZE - sizeof(uint32_t), &bytesRead, NULL)) {
            if (obj->recvHOOK) {
                *(uint32_t*)obj->buffer = bytesRead;
                obj->recvHOOK((void*)obj->buffer);
            }
            MOCIPC_DBGPRINT("[%s] receive len: %d", obj->sharedSVCName, bytesRead);
        }

        FlushFileBuffers(hPipe);
        CloseHandle(hPipe);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
    }

}


void IPCClient::writeImpl(void* src, uint32_t size)
{
    DWORD bytesWritten;
    HANDLE hPipe = createNewPipeInternal(this->sharedCVSName);
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
    CloseHandle(hPipe);
    MOCIPC_DBGPRINT("[%s] write buf len: %d", this->sharedCVSName, bytesWritten);
}

} /* MocIPC client */



struct templateData_t {
    uint8_t id;
    double curve;
    uintptr_t ptr;
    uint8_t data[768];
};

static void serverTest(void* arg)
{
    templateData_t *data = MocIPC::getArg<templateData_t *>(arg);
    uint32_t size = MocIPC::getSize(arg);
    MOCIPC_DBGPRINT("Get id: %d, curve: %.8f, pointer: %llx, total size: %d", data->id, data->curve, data->ptr, size);
}

static void clientTest(void *arg)
{
    templateData_t data = MocIPC::getArg<templateData_t>(arg);
    uint32_t size = MocIPC::getSize(arg);
    MOCIPC_DBGPRINT("Get id: %d, curve: %.8f, pointer: %llx, total size: %d", data.id, data.curve, data.ptr, size);

}



int main()
{
    MocIPC::IPCClient *client_00 = new MocIPC::IPCClient;
    
    MocIPC::IPCServer* server_00 = new MocIPC::IPCServer;
    client_00->registerRecvHOOK(clientTest);
    server_00->registerRecvHOOK(serverTest);
    
    MocIPC::IPCServer* server_01 = new MocIPC::IPCServer("\\\\.\\pipe\\MOCSVC01", "\\\\.\\pipe\\MOCCVS01");
    MocIPC::IPCClient* client_01 = new MocIPC::IPCClient("\\\\.\\pipe\\MOCSVC01", "\\\\.\\pipe\\MOCCVS01");
    client_01->registerRecvHOOK(clientTest);
    server_01->registerRecvHOOK(serverTest);
    MocIPC::IPCServer* server_02 = new MocIPC::IPCServer("\\\\.\\pipe\\MOCSVC", "\\\\.\\pipe\\MOCCVS");
    MocIPC::IPCClient* client_02 = new MocIPC::IPCClient("\\\\.\\pipe\\MOCSVC", "\\\\.\\pipe\\MOCCVS");
    client_02->registerRecvHOOK(clientTest);
    server_02->registerRecvHOOK(serverTest);
    MocIPC::IPCServer* server_10 = new MocIPC::IPCServer("\\\\.\\pipe\\MOCSVC10", "\\\\.\\pipe\\MOCCVS10");
    MocIPC::IPCClient* client_10 = new MocIPC::IPCClient("\\\\.\\pipe\\MOCSVC10", "\\\\.\\pipe\\MOCCVS10");
    client_10->registerRecvHOOK(clientTest);
    server_10->registerRecvHOOK(serverTest);
    MocIPC::IPCServer* server_11 = new MocIPC::IPCServer("\\\\.\\pipe\\MOCSVC11", "\\\\.\\pipe\\MOCCVS11");
    MocIPC::IPCClient* client_11 = new MocIPC::IPCClient("\\\\.\\pipe\\MOCSVC11", "\\\\.\\pipe\\MOCCVS11");
    client_11->registerRecvHOOK(clientTest);
    server_11->registerRecvHOOK(serverTest);
    while (1) {
        templateData_t dataFromServer = { 254, 244.23233534, (uintptr_t)0x152920 };
        server_00->write(&dataFromServer, sizeof(templateData_t));
        server_01->write(&dataFromServer, sizeof(templateData_t));
        Sleep(5000);
        templateData_t dataFromClient = { 127, 122.12358468, (uintptr_t)0x291530 };
        client_00->write(&dataFromClient, sizeof(templateData_t));
        server_01->write(&dataFromClient, sizeof(templateData_t));
        Sleep(5000);
    }
    MOCIPC_DBGPRINT("start");
}
