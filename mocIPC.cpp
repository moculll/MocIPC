#include "mocIPC.h"
#include <iostream>
#include <stdio.h>
#include <future>
#include <random>
#include <ctime>
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

const CHARTYPE* IPCBase::MOCIPC_DEFAULT_SHAREDPREFIX = "\\\\.\\pipe\\MOCSVC";
#endif

namespace Helper {
std::string generateRandomString() {
    std::string result;
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> charDist(0, 61);
    for (int i = 0; i < 4; ++i) {
        int randIndex = charDist(gen);
        if (randIndex < 26) {
            result += 'A' + randIndex;
        }
        else if (randIndex < 52) {
            result += 'a' + (randIndex - 26);
        }
        else {
            result += '0' + (randIndex - 52);
        }
    }
    std::uniform_int_distribution<> numDist(0, 9);
    for (int i = 0; i < 4; ++i) {
        result += '0' + numDist(gen);
    }

    return result;
}
} /* Helper */



IPCBase::IPCBase(const CHARTYPE* _sharedSVCName, const CHARTYPE* _sharedCVSName)
{

    size_t svcSize = STRINGLEN(_sharedSVCName) + 1;
    size_t cvsSize = STRINGLEN(_sharedCVSName) + 1;

    this->recvHOOK = nullptr;

    try {
        this->sharedSVCName = new CHARTYPE[svcSize];
        this->sharedCVSName = new CHARTYPE[cvsSize];
        /* public connection buffer */

    }
    catch (const std::bad_alloc& e) {
        MOCIPC_DBGPRINT("Memory allocation failed: %s, check your memory!", e.what());
        this->sharedSVCName = nullptr;
        this->sharedCVSName = nullptr;
 
        
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
    scb.publicPipe = createNewPipeInternal(this->sharedSVCName);
    if (scb.publicPipe == INVALID_HANDLE_VALUE) {
        MOCIPC_DBGPRINT("Failed to connect to public pipe. Error: %d", GetLastError());
        return;
    }
    this->listenThread = std::thread(&IPCServer::IPCServerListenConnectThreadCallback, this);
    /*ConnectNamedPipe(hPipe, NULL);

    memcpy((void*)buffer, src, size);
    if (!WriteFile(hPipe, (void*)buffer, size, &bytesWritten, NULL)) {
        MOCIPC_DBGPRINT("Failed to write to pipe. Error: %d", GetLastError());
    }
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    MOCIPC_DBGPRINT("[%s] write buf len: %d", this->sharedSVCName, bytesWritten);*/


    this->recvThread = std::thread(&IPCServer::recvThreadCallback, this);
    
}

void IPCServer::IPCServerListenConnectThreadCallback()
{
    exchageMsg_t msg = {0};
    DWORD bytesWritten = 0;
    while (1) {
        /* block function, maybe no need to add a delay */
        ConnectNamedPipe(scb.publicPipe, NULL);
        bytesWritten = 0;
        memset(&msg, 0, sizeof(exchageMsg_t));
        const char *allocResult = IPCServerAllocNewHandle();
        STRCPYSAFE(&msg.serverToClient[0], 256, allocResult);

        if (!WriteFile(scb.publicPipe, (void*)&msg, sizeof(exchageMsg_t), &bytesWritten, NULL)) {
            MOCIPC_DBGPRINT("Failed to write to pipe. Error: %d", GetLastError());
        }
        DisconnectNamedPipe(scb.publicPipe);
        CloseHandle(scb.publicPipe);
        MOCIPC_DBGPRINT("%s connection published, written bytes: %d", msg.serverToClient, bytesWritten);

    }
}


const char *IPCServer::IPCServerAllocNewHandle() {
    std::string randomString = Helper::generateRandomString();
    char tmp[256];
#if UNICODE
    // ½« std::string ×ª»»Îª std::wstring
    std::wstring wRandomString(randomString.begin(), randomString.end());
    swprintf_s(tmp, L"%s%s", MOCIPC_DEFAULT_SHAREDPREFIX, wRandomString.c_str());
#else
    sprintf_s(tmp, "%s%s", MOCIPC_DEFAULT_SHAREDPREFIX, randomString.c_str());
#endif
    
    this->scb.infos.emplace_back(createNewPipeInternal(tmp), std::move(tmp));
    MOCIPC_DBGPRINT("alloced: %s", tmp);
    return this->scb.infos.end()->pipeName;
}


void IPCServer::recvThreadCallback()
{
    HANDLE hPipe;
    DWORD bytesRead;
    while (true) {
        memset(this->scb.infos[0].buffer, 0, BUFFER_BLOCK_SIZE);
        WaitNamedPipe(this->sharedCVSName, NMPWAIT_WAIT_FOREVER);
        hPipe = CreateFile(
            this->sharedCVSName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (ReadFile(hPipe, this->scb.infos[0].buffer + sizeof(uint32_t), BUFFER_BLOCK_SIZE - sizeof(uint32_t), &bytesRead, NULL)) {
            if (this->recvHOOK) {
                *(uint32_t*)this->scb.infos[0].buffer = bytesRead;
                this->recvHOOK((void*)this->scb.infos[0].buffer);
            }

            MOCIPC_DBGPRINT("[%s] receive len: %d", this->sharedCVSName, bytesRead);
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

    memcpy((void*)scb.infos[0].buffer, src, size);
    if (!WriteFile(hPipe, (void*)scb.infos[0].buffer, size, &bytesWritten, NULL)) {
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

void IPCClient::recvThreadCallback()
{
    HANDLE hPipe;
    DWORD bytesRead;
    while (true) {
        WaitNamedPipe(this->sharedSVCName, NMPWAIT_WAIT_FOREVER);
        hPipe = CreateFile(
            this->sharedSVCName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        

        if (ReadFile(hPipe, this->ccb.clientInfos[0].buffer + sizeof(uint32_t), BUFFER_BLOCK_SIZE - sizeof(uint32_t), &bytesRead, NULL)) {
            this->ccb.infos.emplace_back(hPipe, std::move(tmp));
            if (this->recvHOOK) {
                *(uint32_t*)this->ccb.clientInfos[0].buffer = bytesRead;
                this->recvHOOK((void*)this->ccb.clientInfos[0].buffer);
            }
            MOCIPC_DBGPRINT("[%s] receive len: %d", this->sharedSVCName, bytesRead);
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

    memcpy((void*)ccb.clientInfos[0].buffer, src, size);
    if (!WriteFile(hPipe, (void*)ccb.clientInfos[0].buffer, size, &bytesWritten, NULL)) {
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
    
    MocIPC::IPCServer *server_00 = new MocIPC::IPCServer;
    client_00->registerRecvHOOK(clientTest);
    server_00->registerRecvHOOK(serverTest);
    
    
    while (1) {
        templateData_t dataFromServer = { 254, 244.23233534, (uintptr_t)0x152920 };
        server_00->write(&dataFromServer, sizeof(templateData_t));
        Sleep(5000);
        templateData_t dataFromClient = { 127, 122.12358468, (uintptr_t)0x291530 };
        client_00->write(&dataFromClient, sizeof(templateData_t));
        Sleep(5000);
    }
    MOCIPC_DBGPRINT("start");
}
