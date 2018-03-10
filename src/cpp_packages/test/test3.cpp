
#include <iostream>

#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncServerSocket.h>

class AcceptCallback : public folly::AsyncServerSocket::AcceptCallback{
public:
	void connectionAccepted(int fd,
                            const folly::SocketAddress& clientAddr) noexcept override{
		std::cout << "connect a clietn:" << fd << std::endl;
	};

	void acceptError(const std::exception& ex) noexcept override{

	};

	void acceptStarted() noexcept override{

	};
	void acceptStopped() noexcept override{

	};
};



int main(int argc, char** argv){
	auto evb = folly::EventBaseManager::get()->getEventBase();
	auto socket = folly::AsyncServerSocket::newSocket(std::move(evb));
	folly::SocketAddress addr = folly::SocketAddress::makeFromPath("server_socket");
	socket->bind(addr);
	AcceptCallback cb;
	socket->addAcceptCallback(&cb,std::move(evb));
	socket->listen(10);
	socket->startAccepting();
	std::cout << "server preapred" << std::endl;
	evb->loopForever();
}