#include "mocIPC.h"


static void clientRecvTestCallback(void *arg)
{
    MOCIPC_DBGPRINT("client received data!");

}

static void serverRecvTestCallback(void* arg)
{
    MOCIPC_DBGPRINT("server received data!");

}
int main(int argc, char* argv[])
{

    MocIPC::IPCServer *server = new MocIPC::IPCServer();
    MocIPC::IPCClient *client = new MocIPC::IPCClient(MocIPC::IPCDefines::miStdString("\\\\.\\pipe\\MOCIPCDaemon"));
    client->registerRecvHOOK(clientRecvTestCallback);
    server->registerRecvHOOK(serverRecvTestCallback);
    while (1) {
        int a = 15292;
        Sleep(1000);
        server->write(0, &a, sizeof(a));
        client->write(&a, sizeof(a));
        Sleep(1000);
    }
}