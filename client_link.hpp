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
	//-------------------------------------------���Ը�ֵ ��ʼ--------------------------------------------------

	void setPort(const u_short& PORT_) {
		PORT = PORT_;
	}

	void setIP(const wchar_t* ip_) {
		this->ip_ = ip_;
	}

	//����һ�ε��ô˺���
	void init_clientlink(bool isprint = false) {
		init_WSA(isprint);
		init_socket(isprint);
		init_serverinfo();
		connect_server();
	}

	//-------------------------------------------���Ը�ֵ ����--------------------------------------------------

	//-------------------------------------------��ʼ�� ��ʼ--------------------------------------------------
	int init_WSA(bool isprint = false) {
		if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
			if (isprint) {
				std::cerr << "��ʼ��winsock��ʧ��" << std::endl;
			}
			return -1;
		}
		else {
			if (isprint) {
				std::cerr << "��ʼ��winsock��ɹ�" << std::endl;
			}
			return 1;
		}
	}

	int init_socket(bool isprint = false) {
		clientsocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL,0, WSA_FLAG_OVERLAPPED);
		if (clientsocket == INVALID_SOCKET) {
			if (isprint) {
				std::cerr << "socket����ʧ��" << std::endl;
			}
			WSACleanup();
			return -1;
		}
		else {
			if (isprint) {
				std::cerr << "socket�����ɹ�" << std::endl;
			}
		}
	}

	void init_serverinfo() {
		server_addr.sin_port = htons(PORT);
		server_addr.sin_family = AF_INET;

		//windows֧�֣�������ҪPCWSTR���ͣ���ai��Ҫ��ָ����
		InetPton(AF_INET, ip_, &(server_addr.sin_addr));
	}

	int connect_server() {
		std::cerr << "�������ӷ�����..." << std::endl;

		if (connect(clientsocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
			std::cerr << "����ʧ��" << std::endl;
			closesocket(clientsocket);
			WSACleanup();
			return -1;
		}
		else {
			std::cerr << "���ӳɹ��������ӵ�" << &ip_ << std::endl;
		}
	}



	//-------------------------------------------��ʼ�� ����--------------------------------------------------

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
				����ĵڶ�����������һ���Ƚ�������д��������������Ϊʲô����д
				���ȣ���������ڶ���������Ҫ����һ������ָ�롣
				�����Ϊʲô��ʱ����Է���������ȥ����ʱ���ֿ��Է�ָ���ȥ
				һ����˵���ǰ�buffer�Ž�ȥ������������˵��ʵ���ϷŽ�ȥ��һ��ָ�������һ��λ�õ�ָ��
				����������� +���� �õ������λ��ָ��
				Ȼ��Ӻ����λ�ÿ�ʼ����д
			*/

			//���ؽ��յ��ĳ��ȣ����ⲻһ��һ�¾ͽ����꣬��Ȼ��һ���ӽ���������õ�
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

	//ֹͣ�׽��ֵĶ�д����
	void shutdown_() {
		shutdown(clientsocket, SD_BOTH);
	}
	//�ֶ��ر�������Դ
	void close_() {
		closesocket(clientsocket);
		WSACleanup();
	}
};
