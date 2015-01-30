
#include <iostream>
#include "XSFServer.h"
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

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
static std::string expect(const char*& str, const char* match) {

	size_t findIndex = std::string(str).find(match);
	if (findIndex == std::string::npos) {
		throw "down";
	}
	std::string retval = std::string(str).substr(0, findIndex);
	str += findIndex + strlen(match);
	return retval;
}

static void ToLower(std::string& str) {

	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
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
	
	std::vector<unsigned char> WebSocket_Read(bool& closing) {
		std::vector<unsigned char> retval;
		unsigned char op_header;
		str->Read(op_header);
		bool fin = false;
		if ((unsigned char)(op_header >> 7)) {
			fin = true;
		}
		unsigned char opcode = (op_header << 4);
		opcode = opcode >> 4;
		if (opcode == 8) {
			//Terminate connection
			closing = true;
			return retval;
		}
		if (opcode == 2) {
			//TODO: Binary data frame
			//Check mask bit (should be 1, otherwise; protocol violation)
			unsigned char masklen_a;
			str->Read(masklen_a);
			unsigned char maskbit = masklen_a >> 7;
			if (!maskbit) {
				//Protocol violation
				return retval;
			}
			//Compute payload length (part A, first 7 bits)
			masklen_a <<= 1;
			uint64_t len = masklen_a >> 1;
			if (len <= 125) {
				//We know the length
			}
			else {
				if (len == 126) {
					//It's a 16-bit integer (next 16-bit payload)
					uint16_t val;
					str->Read(val);
					NetworkToHost(val);

				}
				else {
					//It's a 64-bit integer (next 64-bit payload)
					throw "NotYetSupported";
				}
			}
			//Now we should have the length; TODO: Read the rest of the packet
			unsigned char mask[4];
			str->Read(mask, 4);
			//NOTICE: Potential attack vector; send a REALLY big value; causing huge malloc
			retval.resize(len);
			str->Read(retval.data(), retval.size());
			for (size_t i = 0; i < len; i++) {
				retval.data()[i] ^= mask[i % 4];
			}


		}
		return retval;
	}

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
		return requestHeaders["upgrade"] == "websocket";
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
			ToLower(name);
			this->requestHeaders[name] = mander;
		}
		std::string connectionStr = this->requestHeaders["connection"];
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
			cstr = "";
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
		while (components[i].find("?") != std::string::npos) {
			size_t pos = components[i].find("?");
			components[i][pos] = '\0';
		}
		retval += components[i];
		if (i < components.size() - 1) {
			retval += "/";
		}
	}
	return retval;
}
static void ProcessWebsocket(std::shared_ptr<HTTPSocket> request) {
	//We're authenticated and trusted.
	//No need for additional checks at this point
	bool key;
	unsigned char opcode;
	bool closed = false;
	std::vector<unsigned char> packet = request->WebSocket_Read(closed);
	try {
		while (!closed) {
			
		}
	}
	catch (const char* er) {

	}
}
static std::string key;
static void process_request(std::shared_ptr<HTTPSocket> request) {
	//Handle HTTP request
	if (request->isWebsocket()) {
		//TODO: Finish later
		//Accept websocket
		//Get the Sec-WebSocket-Key and + the string "258EAFA5-E914-47DA-95CA - C5AB0DC85B11"
		//then SHA1 it, and finally Base64 it
		std::string clikey = request->requestHeaders["sec-websocket-key"];
		clikey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		unsigned char hash[20];
		SHA1((const unsigned char*)clikey.data(), clikey.size(), hash);
		std::string response = Base64(hash, 20);
		request->statusCode = "HTTP/1.1 101 Switching Protocols";
		request->responseHeaders["Sec-WebSocket-Accept"] = response;
		request->responseHeaders["Upgrade"] = "websocket";
		request->responseHeaders["Connection"] = "Upgrade";
		request->BeginResponse();
		bool f = false;
		std::vector<unsigned char> responsedata = request->WebSocket_Read(f);
		//Verify key
		if (responsedata.size()) {

			responsedata.resize(responsedata.size() + 1);
			char* mander = (char*)responsedata.data();
			mander[responsedata.size() - 1] = 0;
			if (mander == key) {
				ProcessWebsocket(request);
			}
		}
		return;
	}
	else {
		try {
			std::string path = GetSafePath(request->path);
			
			if (path == "index.html") {
				if (request->requestHeaders.find("referer") != request->requestHeaders.end()) {
					std::string txt = "This file cannot be viewed from the specified referer.";
					request->statusCode = "HTTP/1.1 403 Forbidden";
					request->SetLength(txt.size());
					request->BeginResponse();
					request->str->Write((unsigned char*)txt.data(), txt.size());
					return;
				}
			}
			if (path == "setx") {
				std::string referer = request->requestHeaders["referer"];
				const char* ptr = referer.data();
				try {
					expect(ptr, "http://127.0.0.1");
					expect(ptr, "/");
					if (std::string(ptr) == "index.html") {
						//Read input from client
						int len = atoi(request->requestHeaders["content-length"].data());
						char* izard = new char[len+1];
						memset(izard, 0, len + 1);
						request->str->Read((unsigned char*)izard, len);
						key = izard;
						delete[] izard;
						request->SetLength(0);
						request->BeginResponse();
						return;
					}
					else {
						throw "down";
					}
				}
				catch (const char* err) {

				}
			}
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