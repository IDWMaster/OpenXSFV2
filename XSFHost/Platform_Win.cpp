// XSFHost.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "XSFServer.h"
#include <Windows.h>
#include <wincrypt.h>
#include <intrin.h>
#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib,"Crypt32.lib")
void NetworkToHost(uint16_t& val) {
	val = ntohs(val);
}
static std::string expect(const char*& str, const char* match) {

	size_t findIndex = std::string(str).find(match);
	if (findIndex == std::string::npos) {
		throw "down";
	}
	std::string retval = std::string(str).substr(0, findIndex);
	str += findIndex + strlen(match);
	return retval;
}

void SHA1(const unsigned char* input, size_t sz, unsigned char* output) {
	HCRYPTPROV provider;
	CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL,0);
	HCRYPTHASH hash;
	CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
	CryptHashData(hash, input, sz, 0);
	DWORD len = 20;
	CryptGetHashParam(hash, HP_HASHVAL, output, &len, 0);
	CryptDestroyHash(hash);
	CryptReleaseContext(provider, 0);

}
std::string Base64(const unsigned char* input, size_t sz) {
	HCRYPTPROV provider;
	CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL, 0);
	DWORD count = 0;
	CryptBinaryToStringA(input, (DWORD)sz, 0, 0, &count);
	char* mander = new char[count];
	CryptBinaryToStringA(input, (DWORD)sz, 0, mander, &count);
	std::string retval = mander;
	delete[] mander;
	const char* ptr = retval.data();
	expect(ptr, "\n");
	retval = expect(ptr, "\r");
	return retval;
}

class IDisposable {
public:
	virtual ~IDisposable(){};
};
class Socket :public IDisposable {
public:
	int s;
	Socket(int s) {
		this->s = s;
	}
	~Socket() {
		closesocket(s);
	}
};
class Server:public IDisposable {
public:
	int s;
	Server(std::function<void(void*)> cb) {
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		struct sockaddr_in ep;
		memset(&ep, 0, sizeof(ep));
		ep.sin_addr.s_addr = inet_addr("127.0.0.1");
		ep.sin_port = htons(85);
		ep.sin_family = AF_INET;
		//ep.
		int res = bind(s, (struct sockaddr*)&ep, sizeof(ep));
		SubmitWork([=](){
			int a = listen(s, 50);
			
			while (true) {
				struct sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;

				int len = sizeof(addr);
				int client = accept(s, (struct sockaddr*)&addr, &len);
				if (client != -1) {
					SubmitWork([=](){
						cb(new Socket(client));
					});
				}
			}
		});
	}
	~Server() {

	}

};
template<typename T>
static T* M(void* other) {
	return (T*)other;
}
void Socket_SendPacket(void* socket, const void* data, size_t len) {
	send(M<Socket>(socket)->s, (const char*)data, len, 0);
	
}

int Socket_Receive(void* socket, void* buffer, size_t len) {
	return recv(M<Socket>(socket)->s, (char*)buffer, len, 0);
}

void* CreateServer(std::function<void(void*)> cb) {
	return new Server(cb);
}
void Dispose(void* obj) {
	delete M<IDisposable>(obj);
}
int _tmain(int argc, _TCHAR* argv[])
{
	WSAData data;
	
	WSAStartup(MAKEWORD(2, 2), &data);
	prog_start();
	Sleep(-1);
	return 0;
}

