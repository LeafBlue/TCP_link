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
	// ȫ����ɶ˿ھ��
	HANDLE g_hCompletionPort = NULL;

	std::mutex result_m;

	std::vector<SOCKET> result_client;

	std::vector<std::string> result_print;

	struct CustomOverlapped {
		OVERLAPPED overlapped;//Ƕ��overlapped
		bool getdata;//Ϊtrueʱ�������ȡ���ǳ��ȣ�Ϊfalseʱ�������ȡ������Ϣ
		int sendflag;
		SOCKET client_socket;
		size_t buffer_len;//����������
		char* buffer;//������ָ��



		CustomOverlapped() : getdata(false), sendflag(-1), buffer_len(0), client_socket(INVALID_SOCKET), buffer(nullptr) {
			ZeroMemory(&overlapped, sizeof(OVERLAPPED));
		}
		~CustomOverlapped() {
			delete[] buffer;
		}

	};
public:
	//---------------------------------------�������� ��ʼ---------------------------------------
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



	//��ȡ�����ӵĿͻ��˵��׽���
	const std::vector<SOCKET> getclient_sockets() {
		return links;
	}

	//��ȡ���յ���Ϣ�Ŀͻ����׽���
	const std::vector<SOCKET> getresult_clientsockets() {
		return result_client;
	}

	//��ȡ���յ���Ϣ�׽��ֶ�Ӧ�±����Ϣ����
	const std::vector<std::string> getresult_clientprints() {
		return result_print;
	}



	//---------------------------------------�������� ����---------------------------------------
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
			std::cerr << "�󶨷�������Ϣʧ��" << std::endl;
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
			std::cerr << "����ʧ��" << std::endl;
			closesocket(serversocket);
			WSACleanup();
			return -1;
		}
		else {
			std::cerr << "������socket��" << serversocket << "���ڼ����˿�" << PORT << std::endl;
		}
		return listen_result;
	}

	void start_accept() {
		accept_(serversocket, g_hCompletionPort);
	}


	//�ؼ��������˺�����װIO����
	void getresult() {
		//�����ʵ�ʴ��͵��ַ���
		DWORD bytesTransferred;
		//���ǰ󶨵���ɶ˿�ʱ�����ݵ���������Ϣ����������һ���׽��֣����������ĸ��ͻ��˴��������֪ͨ
		ULONG_PTR completionKey;
		//����һ��ָ�룬����overlapped�ṹ��
		LPOVERLAPPED overlapped;

		while (true) {
			//��������������ȴ����֪ͨ
			//���ɹ�ʱ���᷵��true����������²�����ֵ
			BOOL success = GetQueuedCompletionStatus(
				g_hCompletionPort, &bytesTransferred, &completionKey, &overlapped, INFINITE);

			//������ɶ˿ڱ��ر�
			if (!overlapped) {
				if (!success) {
					std::cerr << "Completion port closed or error: " << GetLastError() << std::endl;
					break; // �˳�ѭ���������߳�
				}
				continue;
			}

			CustomOverlapped* context = CONTAINING_RECORD(overlapped, CustomOverlapped, overlapped);

			//SOCKET client_socket = (SOCKET)completionKey;//�����������
			SOCKET client_socket = context->client_socket;
			std::cout << "context->sendflag:" << context->sendflag << "--client_socket:" << context->client_socket << "--sendflag��" << context->sendflag << std::endl;

			// ��� client_socket �Ƿ���Ч
			if (client_socket == INVALID_SOCKET) {
				std::cerr << "Invalid client socket in completion" << std::endl;
				delete context;
				continue;
			}
			// ���� AcceptEx��bytesTransferred == 0 ���������
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
			//���õ��Ľ��ת��Ϊ����
			uint32_t message_length = ntohl(*reinterpret_cast<uint32_t*>(context->buffer));

			std::cout << "���յ������ݣ���Ϣ���ȣ�" << message_length << std::endl;
			//������Դ
			delete context;
			recv_(clientSocket, message_length, false);
		}
		else {
			std::string message(context->buffer, bytesTransferred);
			{
				std::lock_guard<std::mutex> l_(result_m);
				result_client.push_back(clientSocket);
				result_print.emplace_back(message);

				std::cout << "���յ������ݣ���Ϣ���ݣ�" << message << std::endl;
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
		//�����״ν���
		recv_(clientSocket, 4, true);

		// �����µ� AcceptEx
		accept_(serversocket, g_hCompletionPort);

	}

	//IOCP�����accept
	void accept_(SOCKET listenSocket, HANDLE completionPort) {
		//�����洢�ͻ����׽���
		SOCKET acceptSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (acceptSocket == INVALID_SOCKET) {
			std::cerr << "WSASocket failed: " << WSAGetLastError() << std::endl;
			return;
		}

		// ���������ĺͻ�����
		CustomOverlapped* context = new CustomOverlapped();
		context->buffer = new char[sizeof(struct sockaddr_in) * 2 + 32];
		context->buffer_len = sizeof(struct sockaddr_in) * 2 + 32;
		context->getdata = false; // ���Ϊ AcceptEx ����
		context->sendflag = 0;    // accept��־
		context->client_socket = acceptSocket;

		HANDLE hResult = CreateIoCompletionPort((HANDLE)acceptSocket, completionPort, (ULONG_PTR)acceptSocket, 0);
		if (hResult == NULL) {
			std::cerr << "CreateIoCompletionPort failed for acceptSocket: " << GetLastError() << std::endl;
			delete context;
			closesocket(acceptSocket);
			return;
		}

		// ��ȡ AcceptEx ����ָ��
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
		context->sendflag = 2;//���ձ�־
		context->buffer_len = len;
		context->client_socket = client_socket;

		DWORD flags = 0; // ��־λ

		WSABUF wsabuf;
		wsabuf.buf = context->buffer;
		wsabuf.len = len;

		//����ʹ��ѭ������������ô������
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
			std::cerr << "WSARecv ������ɣ������ֽ�: " << bytesReceived << std::endl;
		}
	}

	//����������Ҫ������Ϣ����Ҫ���ô˺���
	void send_(SOCKET clientSocket, const std::string& data) {
		CustomOverlapped* context = new CustomOverlapped();


		context->getdata = false;//���Ⱥ���Ϣ����ϵ�һ���͵�

		uint32_t msg_len = data.size();
		int all_len = 4 + msg_len;
		uint32_t switch_len = htonl(msg_len);

		context->buffer = new char[all_len];
		context->sendflag = 1;//���ͱ�־
		context->buffer_len = all_len;
		context->client_socket = clientSocket;

		std::memcpy(context->buffer, &switch_len, sizeof(switch_len));
		std::memcpy(context->buffer + 4, data.c_str(), msg_len);

		WSABUF wsabuf;
		wsabuf.buf = context->buffer;
		wsabuf.len = all_len;

		// �����첽����
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
