#ifndef XSF_HOST
#define XSF_HOST
#include <functional>
#include <memory>
#include "LightThread.h"
#include <string>
/*
XSF Internal Header
*/
void prog_start();
//PLATFORM-DEFINES
void* CreateServer(std::function<void(void*)> onConnection);
void Socket_SendPacket(void* socket, const void* data, size_t len);
int Socket_Receive(void* socket, void* buffer, size_t len);
//20-byte hash
void SHA1(const unsigned char* input, size_t sz, unsigned char* output);
void Dispose(void* object);
std::string Base64(const unsigned char* input, size_t sz);
void NetworkToHost(uint16_t& val);
//END PLATFORM-DEFINES
#endif