#pragma once
#include <iostream>
#include <cstring>
#include <string>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>


#pragma comment(lib,"Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib") 

class server_link {
private:

	WSADATA wsadata;

	u_short PORT;

	SOCKET serversocket;

	struct sockaddr_in server_addr;

	std::vector<SOCKET> links;
	// 全局完成端口句柄
	HANDLE g_hCompletionPort = NULL;

	std::mutex result_m;

	std::vector<SOCKET> result_client;

	std::vector<std::string> result_print;

	struct CustomOverlapped {
		OVERLAPPED overlapped;//嵌入overlapped
		bool getdata;//为true时，代表获取的是长度，为false时，代表获取的是消息
		int sendflag;
		SOCKET client_socket;
		size_t buffer_len;//缓存区长度
		char* buffer;//缓存区指针



		CustomOverlapped() : getdata(false), sendflag(-1), buffer_len(0), client_socket(INVALID_SOCKET), buffer(nullptr) {
			ZeroMemory(&overlapped, sizeof(OVERLAPPED));
		}
		~CustomOverlapped() {
			delete[] buffer;
		}

	};
public:
	//---------------------------------------基本函数 开始---------------------------------------
	server_link() {
		g_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (!g_hCompletionPort) {
			std::cerr << "Failed to create completion port: " << GetLastError() << std::endl;
			return;
		}
	}

	void setPort(const u_short& PORT_) {
		PORT = PORT_;
	}



	//获取已连接的客户端的套接字
	const std::vector<SOCKET> getclient_sockets() {
		return links;
	}

	//获取接收到消息的客户端套接字
	const std::vector<SOCKET> getresult_clientsockets() {
		return result_client;
	}

	//获取接收到消息套接字对应下标的消息内容
	const std::vector<std::string> getresult_clientprints() {
		return result_print;
	}



	//---------------------------------------基本函数 结束---------------------------------------
	void init_(bool isprint = false) {
		init_WSA(isprint);
		init_socket(isprint);
		init_serverinfo();
		bind_();
		listen_();
	}

	int init_WSA(bool isprint = false) {
		if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
			if (isprint) {
				std::cerr << "初始化winsock库失败" << std::endl;
			}
			return -1;
		}
		else {
			if (isprint) {
				std::cerr << "初始化winsock库成功" << std::endl;
			}
			return 1;
		}
	}

	int init_socket(bool isprint = false) {
		serversocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (serversocket == INVALID_SOCKET) {
			std::cerr << "Failed to create socket with error: " << WSAGetLastError() << std::endl;
			return -1;
		}
		else {
			std::cout << "Socket created successfully with WSA_FLAG_OVERLAPPED." << std::endl;
		}
		if (serversocket == INVALID_SOCKET) {
			if (isprint) {
				std::cerr << "socket创建失败" << std::endl;
			}
			WSACleanup();
			return -1;
		}
		else {
			if (isprint) {
				std::cerr << "socket创建成功" << std::endl;
			}
		}
		return serversocket;
	}

	void init_serverinfo() {
		server_addr.sin_port = htons(PORT);
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;


	}

	int bind_() {
		int bind_result = bind(serversocket, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if (bind_result == SOCKET_ERROR) {
			std::cerr << "绑定服务器信息失败" << std::endl;
			closesocket(serversocket);
			WSACleanup();
			return -1;
		}

		HANDLE hResult = CreateIoCompletionPort((HANDLE)serversocket, g_hCompletionPort, (ULONG_PTR)serversocket, 0);
		if (hResult == NULL) {
			std::cerr << "CreateIoCompletionPort failed for listen socket: " << GetLastError() << std::endl;
			closesocket(serversocket);
			return -1;
		}
		return bind_result;
	}

	int listen_() {
		int listen_result = listen(serversocket, SOMAXCONN);
		if (listen_result == SOCKET_ERROR) {
			std::cerr << "监听失败" << std::endl;
			closesocket(serversocket);
			WSACleanup();
			return -1;
		}
		else {
			std::cerr << "服务器socket：" << serversocket << "正在监听端口" << PORT << std::endl;
		}
		return listen_result;
	}

	void start_accept() {
		accept_(serversocket, g_hCompletionPort);
	}


	//关键函数：此函数封装IO操作
	void getresult() {
		//这代表实际传送的字符数
		DWORD bytesTransferred;
		//这是绑定到完成端口时，传递的上下文信息，在这里是一个套接字，用来代表哪个客户端触发了完成通知
		ULONG_PTR completionKey;
		//这是一个指针，叫做overlapped结构，
		LPOVERLAPPED overlapped;

		while (true) {
			//这个函数会阻塞等待完成通知
			//当成功时，会返回true，并填充以下参数的值
			BOOL success = GetQueuedCompletionStatus(
				g_hCompletionPort, &bytesTransferred, &completionKey, &overlapped, INFINITE);

			//可能完成端口被关闭
			if (!overlapped) {
				if (!success) {
					std::cerr << "Completion port closed or error: " << GetLastError() << std::endl;
					break; // 退出循环，结束线程
				}
				continue;
			}

			CustomOverlapped* context = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);

			//SOCKET client_socket = (SOCKET)completionKey;//这个方法不行
			SOCKET client_socket = context->client_socket;
			std::cout << "context->sendflag:" << context->sendflag << "--client_socket:" << context->client_socket << "--sendflag：" << context->sendflag << std::endl;

			// 检查 client_socket 是否有效
			if (client_socket == INVALID_SOCKET) {
				std::cerr << "Invalid client socket in completion" << std::endl;
				delete context;
				continue;
			}
			// 对于 AcceptEx，bytesTransferred == 0 是正常情况
			if (context->sendflag == 0) {
				if (!success) {
					std::cerr << "AcceptEx failed with error: " << GetLastError() << std::endl;
					delete context;
					closesocket(client_socket);
					continue;
				}
				accept_result(client_socket, overlapped);
				continue;
			}

			if (!success || bytesTransferred == 0) {
				DWORD error = GetLastError();
				if (client_socket == serversocket) {
					std::cerr << "Warning: Server socket " << client_socket << " received completion, ignoring." << std::endl;
					delete context;
					continue;
				}
				if (error == ERROR_SUCCESS) {
					std::cerr << "Client " << client_socket << " connection closed gracefully." << std::endl;
				}
				else {
					std::cerr << "Client " << client_socket << " disconnected or error: " << error << std::endl;
					std::lock_guard<std::mutex> lock(result_m);
					links.erase(std::remove(links.begin(), links.end(), client_socket), links.end());
				}
				delete context;
				closesocket(client_socket);
				continue;
			}


			if (context->sendflag == 1) {
				send_result(overlapped);
			}
			else if (context->sendflag == 2) {
				recv_result(client_socket, bytesTransferred, overlapped);
			}
			else {
				accept_result(client_socket, overlapped);
			}
		}
	}


	void send_result(LPOVERLAPPED overlapped) {
		CustomOverlapped* context = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		delete context;
		std::cout << "Send completed" << std::endl;

	}

	void recv_result(SOCKET clientSocket, DWORD bytesTransferred, LPOVERLAPPED overlapped) {
		CustomOverlapped* context = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		if (context->getdata) {
			//将得到的结果转化为数字
			uint32_t message_length = ntohl(*reinterpret_cast<uint32_t*>(context->buffer));

			std::cout << "接收到的内容：消息长度：" << message_length << std::endl;
			//清理资源
			delete context;
			recv_(clientSocket, message_length, false);
		}
		else {
			std::string message(context->buffer, bytesTransferred);
			{
				std::lock_guard<std::mutex> l_(result_m);
				result_client.push_back(clientSocket);
				result_print.emplace_back(message);

				std::cout << "接收到的内容：消息内容：" << message << std::endl;
			}
			delete context;
			recv_(clientSocket, 4, true);
		}
	}

	void accept_result(SOCKET clientSocket, LPOVERLAPPED overlapped) {
		CustomOverlapped* context = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		{
			std::lock_guard<std::mutex> lock(result_m);
			links.push_back(clientSocket);
		}
		delete context;
		//启动首次接收
		recv_(clientSocket, 4, true);

		// 发起新的 AcceptEx
		accept_(serversocket, g_hCompletionPort);

	}

	//IOCP管理的accept
	void accept_(SOCKET listenSocket, HANDLE completionPort) {
		//用来存储客户端套接字
		SOCKET acceptSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (acceptSocket == INVALID_SOCKET) {
			std::cerr << "WSASocket failed: " << WSAGetLastError() << std::endl;
			return;
		}

		// 分配上下文和缓冲区
		CustomOverlapped* context = new CustomOverlapped();
		context->buffer = new char[sizeof(struct sockaddr_in) * 2 + 32];
		context->buffer_len = sizeof(struct sockaddr_in) * 2 + 32;
		context->getdata = false; // 标记为 AcceptEx 操作
		context->sendflag = 0;    // accept标志
		context->client_socket = acceptSocket;

		HANDLE hResult = CreateIoCompletionPort((HANDLE)acceptSocket, completionPort, (ULONG_PTR)acceptSocket, 0);
		if (hResult == NULL) {
			std::cerr << "CreateIoCompletionPort failed for acceptSocket: " << GetLastError() << std::endl;
			delete context;
			closesocket(acceptSocket);
			return;
		}

		// 获取 AcceptEx 函数指针
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
		DWORD bytes = 0;
		WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&lpfnAcceptEx, sizeof(lpfnAcceptEx), &bytes, NULL, NULL);

		BOOL result = lpfnAcceptEx(listenSocket, acceptSocket, context->buffer,
			0, sizeof(struct sockaddr_in) + 16,
			sizeof(struct sockaddr_in) + 16, &bytes, &(context->overlapped));

		if (!result && WSAGetLastError() != WSA_IO_PENDING) {
			std::cerr << "AcceptEx failed: " << WSAGetLastError() << std::endl;
			delete context;
			closesocket(acceptSocket);
		}
		else {
			std::cout << "Async accept initiated for socket: " << acceptSocket << std::endl;
		}
	}

	void recv_(SOCKET& client_socket, int len, bool getdatalag) {
		CustomOverlapped* context = new CustomOverlapped();
		context->buffer = new char[len];

		context->getdata = getdatalag;
		context->sendflag = 2;//接收标志
		context->buffer_len = len;
		context->client_socket = client_socket;

		DWORD flags = 0; // 标志位

		WSABUF wsabuf;
		wsabuf.buf = context->buffer;
		wsabuf.len = len;

		//不再使用循环，不可能那么多数据
		DWORD bytesReceived = 0;
		int result = WSARecv(client_socket, &wsabuf, 1, &bytesReceived, &flags, &context->overlapped, NULL);
		if (result == SOCKET_ERROR) {
			DWORD error = WSAGetLastError();
			if (error != WSA_IO_PENDING) {
				std::cerr << "WSARecv failed with error: " << error << " (WSA Error Code)" << std::endl;
				delete context;
				return;
			}
		}
		else {
			std::cerr << "WSARecv 立即完成，接收字节: " << bytesReceived << std::endl;
		}
	}

	//如果服务端需要发送消息，需要调用此函数
	void send_(SOCKET clientSocket, const std::string& data) {
		CustomOverlapped* context = new CustomOverlapped();


		context->getdata = false;//长度和消息是组合到一起发送的

		uint32_t msg_len = data.size();
		int all_len = 4 + msg_len;
		uint32_t switch_len = htonl(msg_len);

		context->buffer = new char[all_len];
		context->sendflag = 1;//发送标志
		context->buffer_len = all_len;
		context->client_socket = clientSocket;

		std::memcpy(context->buffer, &switch_len, sizeof(switch_len));
		std::memcpy(context->buffer + 4, data.c_str(), msg_len);

		WSABUF wsabuf;
		wsabuf.buf = context->buffer;
		wsabuf.len = all_len;

		// 发起异步发送
		DWORD bytesSent = 0;
		int result = WSASend(clientSocket, &wsabuf, 1, &bytesSent, 0, &context->overlapped, NULL);
		if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
			delete context;
		}
		else {
			std::cout << "Async send initiated for socket: " << clientSocket << std::endl;
		}
	}

};
