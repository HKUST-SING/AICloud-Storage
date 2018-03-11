/*
 * IPCContext includes the argument needed in IPCServer
 */
#include <folly/SocketAddress.h> 

#include "callback/ServerAcceptCallback.h"
#include "callback/ClientConnectionCallback.h"

namespace singaistorageipc{

class IPCContext{
public:
	IPCContext(std::string s, int backlog){
		addr_ = folly::SocketAddress::makeFromPath(s);
		backlog_ = backlog;
	};

	folly::SocketAddress addr_;

	ServerAcceptCallback scb_;

	ClientConnectionCallback ccb_;

	int backlog_;
};

}