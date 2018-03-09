#include "lib/IPCServer.h"
#include "lib/IPCSocketPoolContext.h"

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>

#include <iostream>

using namespace singaistorageipc;

typedef std::function<void()> VoidCallback;

class ConnCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  ConnCallback()
      : state(STATE_WAITING),
        exception(folly::AsyncSocketException::UNKNOWN, "none") {}

  void connectSuccess() noexcept override {
    state = STATE_SUCCEEDED;
    if (successCallback) {
      successCallback();
    }
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    state = STATE_FAILED;
    exception = ex;
    if (errorCallback) {
      errorCallback();
    }
  }

  StateEnum state;
  folly::AsyncSocketException exception;
  VoidCallback successCallback;
  VoidCallback errorCallback;
};

int main(){
	IPCSocketPoolContext context = new IPCSocketPoolContext();
	IPCServer server = new IPCServer(context);

	folly::SocketAddress addr = folly::SocketAddress::makeFromPath("server_socket");
	std::vector<folly::SocketAddress> a = {addr};

	server.bind(addr);
	std::thread t([&](){
		server.start();
	});

	EventBase evb;
	std::shared_ptr<AsyncSocket> socket = AsyncSocket::newSocket(&evb);
	ConnCallback cb;
	cb.successCallback = [](){std::cout<<"client success\n"};
	cb.errorCallback = [](){std::cout<<"client fail\n"};
	socket->connect(&cb,addr,30);
	evb.loop();

	return 0;
}