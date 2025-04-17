#include"client_link.hpp"
#include <thread>

int main() {

	client_link client;
	client.setPort(9092);
	client.setIP(L"127.0.0.1");

	client.init_clientlink(true);

	std::thread t0([&client]() {
		while (true) {
			std::string message;
			std::getline(std::cin, message);
			if (message == "exit") break;
			client.send_(message);
		}
	});

	std::thread t1([&client]() {
		while (true) {
			std::string msg = client.recv_();
			std::cout << msg << std::endl;
		}
	});

	t0.join();
	t1.join();

	/*
	#include"server_link.hpp"


int main() {

	server_link server_;
	server_.setPort(9092);
	std::cout << server_.init_WSA(isprint) << std::endl;
	std::cout<<server_.init_socket(isprint) << std::endl;
	server_.init_serverinfo() ;
	std::cout<<server_.bind_() << std::endl;
	std::cout<<server_.listen_() << std::endl;

	std::thread t0([&server_]() {
		server_.getresult();
	});

	server_.start_accept();

	std::thread t1([&server_]() {
		const std::vector<SOCKET> lv_ = server_.getclient_sockets();
		if (!lv_.empty()) {
			while (true) {
				std::string message;
				std::getline(std::cin, message);

				server_.send_(lv_[0], message);
			}
		}
	});

	t0.join();
	t1.join();
}

	*/
}