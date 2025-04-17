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
		std::shared_ptr<char[]> buffer;//缓存区指针

		CustomOverlapped() : getdata(false), sendflag(0) {
			ZeroMemory(&overlapped, sizeof(OVERLAPPED));
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
			std::cerr << "服务器正在监听端口" << PORT << std::endl;
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

			SOCKET client_socket = (SOCKET)completionKey;
			CustomOverlapped* context_ = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
			std::shared_ptr<CustomOverlapped> context(context_);

			std::cout << context_->sendflag << "---" << context_->getdata << std::endl;//输出了0---0

			//客户端断开连接或 IO 错误
			if (!success || bytesTransferred == 0) {
				DWORD error = GetLastError();
				if (error == ERROR_SUCCESS) {
					// 忽略非致命错误
					std::cerr << "Client " << client_socket << " connection closed gracefully." << std::endl;
				}
				else {
					std::cerr << "Client " << client_socket << " disconnected or error: " << error << std::endl;
					// 从 links 中移除 client_socket
					{
						std::lock_guard<std::mutex> lock(result_m);
						links.erase(
							std::remove(links.begin(), links.end(), client_socket),
							links.end()
						);
					}
					// 关闭套接字
					closesocket(client_socket);
				}
				// 释放资源
				delete overlapped;
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
		CustomOverlapped* context_ = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		std::shared_ptr<CustomOverlapped> context(context_);

		std::cout << "Send completed" << std::endl;

		// 清理资源
		delete overlapped;
	}

	void recv_result(SOCKET clientSocket, DWORD bytesTransferred, LPOVERLAPPED overlapped) {
		CustomOverlapped* context_ = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		std::shared_ptr<CustomOverlapped> context(context_);
		if (context->getdata) {
			//将得到的结果转化为数字
			uint32_t message_length = ntohl(*reinterpret_cast<uint32_t*>(context->buffer.get()));
			//清理资源
			delete overlapped;

			recv_(clientSocket, message_length, false);
		}
		else {
			{
				std::lock_guard<std::mutex> l_(result_m);
				result_client.push_back(clientSocket);
				result_print.emplace_back(std::string(context->buffer.get()));
			}
			delete overlapped;

			recv_(clientSocket, 4, true);
		}
	}

	void accept_result(SOCKET clientSocket, LPOVERLAPPED overlapped) {
		CustomOverlapped* context_ = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);
		std::shared_ptr<CustomOverlapped> context(context_);

		HANDLE hResult = CreateIoCompletionPort((HANDLE)clientSocket, g_hCompletionPort, (ULONG_PTR)clientSocket, 0);
		if (hResult == NULL) {
			std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
			closesocket(clientSocket);
			delete overlapped;
			return;
		}

		{
			std::lock_guard<std::mutex> lock(result_m);
			links.push_back(clientSocket);
		}
		//启动首次接收
		recv_(clientSocket, 4, true);

		delete overlapped;
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
		std::shared_ptr<CustomOverlapped> context = std::make_shared<CustomOverlapped>();
		context->buffer = std::shared_ptr<char[]>(new char[sizeof(struct sockaddr_in) * 2 + 32]);
		context->getdata = false; // 标记为 AcceptEx 操作
		context->sendflag = 0;    // accept标志

		// 获取 AcceptEx 函数指针
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
		DWORD bytes = 0;
		WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&lpfnAcceptEx, sizeof(lpfnAcceptEx), &bytes, NULL, NULL);

		BOOL result = lpfnAcceptEx(listenSocket, acceptSocket, context->buffer.get(),
			0, sizeof(struct sockaddr_in) + 16,
			sizeof(struct sockaddr_in) + 16, &bytes, &context->overlapped);

		if (!result && WSAGetLastError() != WSA_IO_PENDING) {
			std::cerr << "AcceptEx failed: " << WSAGetLastError() << std::endl;
			closesocket(acceptSocket);
		}
		else {
			std::cout << "Async accept initiated for socket: " << acceptSocket << std::endl;
		}
	}

	void recv_(SOCKET& client_socket, int len, bool getdatalag) {
		std::shared_ptr<CustomOverlapped> context = std::make_shared<CustomOverlapped>();
		context->buffer = std::shared_ptr<char[]>(new char[len]);

		context->getdata = getdatalag;
		context->sendflag = 2;//接收标志

		DWORD flags = 0; // 标志位

		WSABUF wsabuf;
		wsabuf.buf = context->buffer.get();
		wsabuf.len = len;

		//不再使用循环，不可能那么多数据
		DWORD bytesReceived = 0;
		int result = WSARecv(client_socket, &wsabuf, 1, &bytesReceived, &flags, &context->overlapped, NULL);
		std::cerr << "-----" << wsabuf.buf << std::endl;
		if (result == SOCKET_ERROR) {
			DWORD error = WSAGetLastError();
			if (error != WSA_IO_PENDING) {
				std::cerr << "WSARecv failed with error: " << error << " (WSA Error Code)" << std::endl;
				return;
			}
		}
		else {
			std::cerr << "WSARecv 立即完成，接收字节: " << bytesReceived << std::endl;
		}
	}

	//如果服务端需要发送消息，需要调用此函数
	void send_(SOCKET clientSocket, const std::string& data) {
		std::shared_ptr<CustomOverlapped> context = std::make_shared<CustomOverlapped>();

		context->getdata = false;//长度和消息是组合到一起发送的

		uint32_t msg_len = data.size();
		int all_len = 4 + msg_len;
		uint32_t switch_len = htonl(msg_len);

		context->buffer = std::shared_ptr<char[]>(new char[all_len]);
		context->sendflag = 1;//发送标志

		std::memcpy(context->buffer.get(), &switch_len, sizeof(switch_len));
		std::memcpy(context->buffer.get() + 4, data.c_str(), msg_len);

		WSABUF wsabuf;
		wsabuf.buf = context->buffer.get();
		wsabuf.len = all_len;

		// 发起异步发送
		DWORD bytesSent = 0;
		int result = WSASend(clientSocket, &wsabuf, 1, &bytesSent, 0, &context->overlapped, NULL);
		if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
		}
		else {
			std::cout << "Async send initiated for socket: " << clientSocket << std::endl;
		}
	}

};
