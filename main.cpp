#include "mocIPC.h"


static void clientRecvTestCallback(void *arg)
{


    printf("client received data: %d, size: %d\n", MocIPC::getArg<int>(arg), MocIPC::getSize(arg));

}

static void serverRecvTestCallback(void* arg)
{
    int a = MocIPC::getArg<int>(arg);
    printf("server received data: %d, size: %d\n", MocIPC::getArg<int>(arg), MocIPC::getSize(arg));

}
int main(int argc, char* argv[])
{
    std::vector<MocIPC::IPCClient*> clientList;
    for (int i = 0; i < 200; i++) {
        MocIPC::IPCClient* client = new MocIPC::IPCClient(MocIPC::IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDeamon"));
        clientList.emplace_back(client);
        client->registerRecvHOOK(clientRecvTestCallback);
    }
    
    MocIPC::IPCServer *server = new MocIPC::IPCServer();

    server->registerRecvHOOK(serverRecvTestCallback);


    while (1) {
        int clientIndex = 0;
        for (int i = 0; i < 200; i++) {
            server->write(i, &clientIndex, sizeof(clientIndex));
            ++clientIndex;
        }
        
        
        Sleep(1);
    }
}