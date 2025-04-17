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
	
	*/
}