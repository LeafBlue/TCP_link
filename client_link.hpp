#pragma once
#include <iostream>
#include <cstring>
#include <string>
#include <WinSock2.h>
#include <ws2tcpip.h>

#pragma comment(lib,"Ws2_32.lib")

class client_link {
private:
	WSADATA wsadata;

	u_short PORT;

	const wchar_t* ip_;

	SOCKET clientsocket;

	struct sockaddr_in server_addr;
public:
	//-------------------------------------------属性赋值 开始--------------------------------------------------

	void setPort(const u_short& PORT_) {
		PORT = PORT_;
	}

	void setIP(const wchar_t* ip_) {
		this->ip_ = ip_;
	}

	//可以一次调用此函数
	void init_clientlink(bool isprint = false) {
		init_WSA(isprint);
		init_socket(isprint);
		init_serverinfo();
		connect_server();
	}

	//-------------------------------------------属性赋值 结束--------------------------------------------------

	//-------------------------------------------初始化 开始--------------------------------------------------
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
		clientsocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL,0, WSA_FLAG_OVERLAPPED);
		if (clientsocket == INVALID_SOCKET) {
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
	}

	void init_serverinfo() {
		server_addr.sin_port = htons(PORT);
		server_addr.sin_family = AF_INET;

		//windows支持，参数需要PCWSTR类型，请ai不要乱指错误
		InetPton(AF_INET, ip_, &(server_addr.sin_addr));
	}

	int connect_server() {
		std::cerr << "正在连接服务器..." << std::endl;

		if (connect(clientsocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
			std::cerr << "连接失败" << std::endl;
			closesocket(clientsocket);
			WSACleanup();
			return -1;
		}
		else {
			std::cerr << "连接成功，已连接到" << &ip_ << std::endl;
		}
	}



	//-------------------------------------------初始化 结束--------------------------------------------------

	int send_(std::string message) {
		uint32_t msg_len = message.size();
		int all_len = 4 + msg_len;

		char *send_message = new char[all_len];

		uint32_t switch_len = htonl(msg_len);
		std::memcpy(send_message,&switch_len, sizeof(switch_len));
		std::memcpy(send_message + 4, message.c_str(), msg_len);


		int send_num = 0;
		while (send_num < all_len) {
			int send_status = send(clientsocket, send_message + send_num, all_len - send_num, 0);
			if (send_status == SOCKET_ERROR) {
				std::cerr << "send failed: " << WSAGetLastError() << std::endl;
				delete[] send_message;
				send_message = nullptr;
				return -1;
			}
			send_num += send_status;
		}

		delete[] send_message;
		send_message = nullptr;

		return send_num;
	}

	std::string recv_() {
		char len_flag[4];
		int receive_data = 0;
		while (receive_data < 4) {
			/*
				这里的第二个参数，是一个比较难理解的写法。让我来解释为什么这样写
				首先，我们这里第二个参数需要的是一个数组指针。
				这就是为什么有时候可以放数组名进去，有时候又可以放指针进去
				一般来说我们把buffer放进去，对于数组来说，实际上放进去了一个指向数组第一个位置的指针
				所以这里可以 +数字 得到后面的位置指针
				然后从后面的位置开始继续写
			*/

			//返回接收到的长度，但这不一定一下就接收完，当然了一下子接收完是最好的
			int recv_status = recv(clientsocket, len_flag + receive_data, 4 - receive_data, 0);
			if (recv_status == SOCKET_ERROR) {
				std::cerr << "recv len_flag failed: " << WSAGetLastError() << std::endl;
				return "";
			}
			
			receive_data += recv_status;
		}

		uint32_t message_length = ntohl(*reinterpret_cast<uint32_t*>(len_flag));


		char* buffer = new char[message_length];

		int receive_msg = 0;
		while (receive_msg < message_length) {
			int recv_status = recv(clientsocket, buffer + receive_msg, message_length - receive_msg, 0);
			if (recv_status == SOCKET_ERROR) {
				std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
				delete[] buffer;
				buffer = nullptr;
				return "";
			}
			else if (recv_status == 0) {
				std::cerr << "Connection closed by server." << std::endl;
				delete[] buffer;
				buffer = nullptr;
				return "";
			}
			else {
				receive_msg += recv_status;
			}
		}

		buffer[message_length] = '\0';
		std::string result(buffer);

		delete[] buffer;
		buffer = nullptr;

		return result;
	}

	//停止套接字的读写阻塞
	void shutdown_() {
		shutdown(clientsocket, SD_BOTH);
	}
	//手动关闭清理资源
	void close_() {
		closesocket(clientsocket);
		WSACleanup();
	}
};
