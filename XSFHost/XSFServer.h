#ifndef XSF_HOST
#define XSF_HOST
#include <functional>
#include <memory>
#include "LightThread.h"
/*
XSF Internal Header
*/
void prog_start();
//PLATFORM-DEFINES
void* CreateServer(std::function<void(void*)> onConnection);
void Socket_SendPacket(void* socket, const void* data, size_t len);
int Socket_Receive(void* socket, void* buffer, size_t len);

void Dispose(void* object);
//END PLATFORM-DEFINES
#endif