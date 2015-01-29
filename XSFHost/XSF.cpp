
#include <iostream>
#include "XSFServer.h"
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
class SocketStream {
public:
	size_t packetLength;
	unsigned char buffer[1024 * 90];
	size_t offset;
	void* s;
	SocketStream(void* socket) {
		s = socket;
		packetLength = 0;
		offset = 0;
	}
	void Read(unsigned char* bytes, size_t len) {
		while (len != 0) {
			size_t avail = std::min(len, packetLength-offset);
			memcpy(bytes, buffer + offset, avail);
			len -= avail;
			offset += avail;
			if (len) {
				packetLength = Socket_Receive(s, buffer, 1024 * 90);
				if ((packetLength > 1024 * 90) || packetLength == 0) {
					//Barf
					throw "up";
				}
				offset = 0;
			}
		}
	}
	void Write(unsigned char* bytes, size_t len) {
		Socket_SendPacket(s, bytes, len);
	}
	template<typename T>
	void Write(const T& val) {
		Write((unsigned char*)&val, sizeof(val));
	}
	template<typename T>
	void Read(T& val) {
		Read((unsigned char*)&val, sizeof(val));
	}
	~SocketStream() {
		Dispose(s);
	}
};
//What to do when you're expecting
std::string expect(const char*& str, const char* match) {

	size_t findIndex = std::string(str).find(match);
	if (findIndex == std::string::npos) {
		throw "down";
	}
	std::string retval = std::string(str).substr(0, findIndex);
	str += findIndex + strlen(match);
	return retval;
}


//Forward-declaration for socket
class HTTPSocket;
static void process_request(std::shared_ptr<HTTPSocket> request);
class HTTPSocket {
public:
	std::string method;
	std::string path;
	std::shared_ptr<SocketStream> str;
	std::map<std::string, std::string> requestHeaders;
	std::map<std::string, std::string> responseHeaders;
	std::string statusCode;
	

	void BeginResponse() {
		std::string txt = statusCode+"\r\n";
		str->Write((unsigned char*)txt.data(), txt.size());
		for (auto it = responseHeaders.begin();it != responseHeaders.end(); it++) {
			txt = it->first;
			txt += ": ";
			txt += it->second;
			txt += "\r\n";
			str->Write((unsigned char*)txt.data(), txt.size());
		}
		txt = "\r\n";
		str->Write((unsigned char*)txt.data(), txt.size());
	}
	bool isWebsocket() {
		return requestHeaders["Upgrade"] == "websocket";
	}
	void SetLength(size_t len) {
		std::stringstream m;
		m << len;
		responseHeaders["Content-Length"] = m.str();
	}
	~HTTPSocket() {
		if (responseHeaders["Connection"] == "keep-alive") {
			try {
				std::shared_ptr<HTTPSocket> ms = std::make_shared<HTTPSocket>(str);
				//If we didn't run this on a separate thread; a client could make us
				//http://StackOverflow.com
				SubmitWork([=](){
					process_request(ms);
				});
			}
			catch (const char* err) {

			}
		}
	}
	//Newline = \r\n
	//Last line = no content then \r\n
	HTTPSocket(std::shared_ptr<SocketStream> s) {
		statusCode = "HTTP/1.1 200 OK";
		responseHeaders["Content-Type"] = "text/html";
		str = s;
		//Read in HTTP headers
		char mander;
		std::string mval;
		std::vector<std::string> headers;
		while (true) {
			str->Read(mander);
			if (mander == '\r') {
				char next;
				str->Read(next);
				if (next == '\n') {
					//End of header
					if (mval.size()) {
						headers.push_back(mval);
						mval = "";
					}
					else {
						break;
					}
				}

			}
			else {
				mval += mander;
			}

		}


		std::string reqPath = headers[0];
		/*size_t incr = reqPath.find(' ');
		method = reqPath.substr(0, incr);
		reqPath = std::string(reqPath.data() + incr+1);
		path = reqPath.substr(0, reqPath.find(' '));*/
		const char* ptr = reqPath.data();
		method = expect(ptr, " ");
		path = expect(ptr," ");
		for (size_t i = 1; i < headers.size(); i++) {
			std::string m = headers[i];
			const char* mander = m.data();
			std::string name = expect(mander, ": ");
			this->requestHeaders[name] = mander;
		}
		std::string connectionStr = this->requestHeaders["Connection"];
		if (connectionStr == "keep-alive") {
			responseHeaders["Connection"] = "keep-alive";
		}
		else {
			responseHeaders["Connection"] = "close";
		}
	}
};
static std::string GetSafePath(std::string requestURL) {
	const char* current = requestURL.data()+1;
	std::vector<std::string> components;
	std::string cstr;
	while (*current != 0) {
		if (*current == '/') {
			components.push_back(cstr);
		}
		else {
			cstr += *current;
		}
		current++;
	}
	components.push_back(cstr);
	for (size_t i = 0; i < components.size()-1; i++) {
		if (components[i].find(".") != std::string::npos) {
			throw "down";
		}
	}
	std::string retval;
	for (size_t i = 0; i < components.size(); i++) {
		retval += components[i];
		if (i < components.size() - 1) {
			retval += "/";
		}
	}
	return retval;
}
static void process_request(std::shared_ptr<HTTPSocket> request) {
	//Handle HTTP request
	if (request->isWebsocket()) {
		//TODO: Finish later
		//Accept websocket
		//Get the Sec-WebSocket-Key and + the string "258EAFA5-E914-47DA-95CA - C5AB0DC85B11"
		//then SHA1 it, and finally Base64 it
		std::string clikey = request->requestHeaders["Sec-WebSocket-Key"];
		clikey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		unsigned char hash[20];
		SHA1((const unsigned char*)clikey.data(), clikey.size(), hash);
		Base64(hash, 20);
	}
	else {
		try {
			std::string path = GetSafePath(request->path);

			std::ifstream file;

			file.open(path, std::ios::binary | std::ios::in);
			if (!file.good()) {

				std::string txt = "Resource not found.";
				request->SetLength(txt.size());
				request->statusCode = "HTTP/1.1 404 Not Found";
				request->BeginResponse();
				request->str->Write((unsigned char*)txt.data(), txt.size());
				return;
			}
			const char* m = path.data();
			expect(m, ".");
			std::string extension = m;
			if (extension == "html") {
				request->responseHeaders["Content-Type"] = "text/html";

			}
			else {
				if (extension == "js") {
					request->responseHeaders["Content-Type"] = "text/javascript";
				}
			}
			file.seekg(0, std::ios::end);
			request->SetLength(file.tellg());
			request->BeginResponse();
			file.seekg(0, std::ios::beg);
			
			unsigned char buffer[1024 * 5];
			while (!file.eof()) {
				file.read((char*)buffer, 1024 * 5);
				request->str->Write(buffer, file.gcount());
			}
		}
		catch (const char* err) {

		} 
	}
}

void prog_start() {
	void* server = CreateServer([=](void* socket){
		try {
			std::shared_ptr<HTTPSocket> request = std::make_shared<HTTPSocket>(std::make_shared<SocketStream>(socket));
			process_request(request);
		}
		catch (const char* err){
			
		}
	});
}