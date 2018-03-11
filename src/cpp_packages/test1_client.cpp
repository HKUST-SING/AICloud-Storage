#include "lib/Callback.h"

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/IOBuf.h>

#include <iostream>

using namespace singaistorageipc;

typedef std::function<void()> VoidCallback;

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
	std::cout << ex.what() << std::endl;
    }
  }

  folly::AsyncSocketException exception;
  VoidCallback successCallback;
  VoidCallback errorCallback;
};

int main(int argc, char** argv){

	folly::SocketAddress addr = folly::SocketAddress::makeFromPath("server_socket");

	folly::EventBase evb;
	std::shared_ptr<folly::AsyncSocket> socket = folly::AsyncSocket::newSocket(&evb);
	ConnCallback cb;
	cb.successCallback = [](){std::cout<<"client success\n";};
	cb.errorCallback = [](){std::cout<<"client fail\n";};
	socket->connect(&cb,addr,30);

  ServerWriteCallback wcb(socket->getFd());
  std::string data = "pssadd";
  auto buf = folly::IOBuf::copyBuffer(data.c_str(), data.length());
  socket->write(&wcb,std::move(buf));

	evb.loop();

	return 0;
}
