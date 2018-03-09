//#include "lib/IPCServer.h"
//#include "lib/IPCSocketPoolContext.h"

#include <folly/SocketAddress.h>
//#include <folly/io/async/AsyncSocket.h>
//#include <folly/io/async/EventBase.h>

#include <iostream>

//using namespace singaistorageipc;

//typedef std::function<void()> VoidCallback;
/*
class ConnCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  ConnCallback()
        :exception(folly::AsyncSocketException::UNKNOWN, "none") {}

  void connectSuccess() noexcept override {
    if (successCallback) {
      successCallback();
    }
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    exception = ex;
    if (errorCallback) {
      errorCallback();
    }
  }

  folly::AsyncSocketException exception;
  VoidCallback successCallback;
  VoidCallback errorCallback;
};
*/
int main(int argc, char** argv){
	//IPCSocketPoolContext context;
	//IPCServer server(context);
	std::cout << "h" << std::endl;
	folly::SocketAddress addr = folly::SocketAddress::makeFromPath("server_socket");
	//std::vector<folly::SocketAddress> a = {addr};
/*
	server.bind(a);
	std::thread t([&](){
		server.start();
	});

	folly::EventBase evb;
	std::shared_ptr<folly::AsyncSocket> socket = folly::AsyncSocket::newSocket(&evb);
	ConnCallback cb;
	cb.successCallback = [](){std::cout<<"client success\n";};
	cb.errorCallback = [](){std::cout<<"client fail\n";};
	socket->connect(&cb,addr,30);
	evb.loop();
*/
	return 0;
}
