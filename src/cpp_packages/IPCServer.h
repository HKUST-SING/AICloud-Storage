/* Can test
 * IPC server object.
 */
#pragma once

/**
 * External dependence
 */
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>

/**
 * Internal dependence
 */
#include "IPCContext.h"

namespace singaistorageipc{

class IPCServer final{
public:
	IPCServer() = delete;
	IPCServer(IPCContext context):context_(context){
		evb_ = folly::EventBaseManager::get()->getEventBase();
		socket_ = folly::AsyncServerSocket::newSocket(evb_);
	};

	void start();
    void stop();

private:
	IPCContext context_;
	folly::EventBase *evb_;
	std::shared_ptr<folly::AsyncServerSocket> socket_;
};

}
