#include "mocIPC.h"


namespace {

struct testdat_t {
    int a;
    double b;
    char c;
} testdatServer, testdatClient;
static void clientRecvTestCallback(void *arg)
{
    printf("client received { a: %d, b: %.3f, c: 0x%02x }, size: %d\n", MocIPC::getArg<testdat_t>(arg).a, MocIPC::getArg<testdat_t>(arg).b, MocIPC::getArg<testdat_t>(arg).c, MocIPC::getSize(arg));

}

static void serverRecvTestCallback(void* arg)
{
    printf("server received { a: %d, b: %.3f, c: 0x%02x }, size: %d\n", MocIPC::getArg<testdat_t>(arg).a, MocIPC::getArg<testdat_t>(arg).b, MocIPC::getArg<testdat_t>(arg).c, MocIPC::getSize(arg));

}

} /* anonymous namespace */

int main(int argc, char* argv[])
{
    /*std::vector<MocIPC::IPCClient*> clientList;
    for (int i = 0; i < 512; i++) {
        MocIPC::IPCClient *client = new MocIPC::IPCClient(MocIPC::IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDeamon"));
        clientList.emplace_back(client);
        client->registerRecvHOOK(clientRecvTestCallback);
    }
    
    MocIPC::IPCServer *server = new MocIPC::IPCServer();

    server->registerRecvHOOK(serverRecvTestCallback);
    Sleep(500);
    while (1) {
        int clientIndex = 0;
        for (int i = 0; i < 512; i++) {
            server->write(i, &clientIndex, sizeof(clientIndex));
            ++clientIndex;
            Sleep(30);
        }

    }*/
    MocIPC::IPCServer* server = new MocIPC::IPCServer();
    MocIPC::IPCClient* client = new MocIPC::IPCClient();

    client->registerRecvHOOK(clientRecvTestCallback);
    server->registerRecvHOOK(serverRecvTestCallback);
   
    testdatServer = {1, 2.345, 0x67};
    testdatClient = { 8, 9.101, 0x11 };
    while (1) {
        server->write(0, &testdatServer, sizeof(testdat_t));
        client->write(0, &testdatClient, sizeof(testdat_t));
        Sleep(500);
    }
}