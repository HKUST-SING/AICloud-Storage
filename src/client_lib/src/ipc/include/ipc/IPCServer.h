/* Can test
 * IPC server object.
 */
#pragma once
/**
 * Standard dependence
 */
#include <csignal>

/**
 * External dependence
 */
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>

/**
 * Internal dependence
 */
#include "lib/callback/ServerAcceptCallback.h"
#include "lib/callback/ClientConnectionCallback.h"
#include "IPCContext.h"
#include "SysSignalHandler.h"

namespace singaistorageipc{

class IPCServer final{
public:
	IPCServer() = delete;
	IPCServer(IPCContext context)
	:evb_(folly::EventBaseManager::get()->getEventBase()),
	context_(context),
	scb_(context_.bufferSize_,context_.minAllocBuf_,
	    context_.newAllocSize_,context_.readSMSize_,
        context_.writeSMSize_,context_.addr_.getPath(),
        context_.socketsMap_,context_.sec_,context_.worker_),
	ccb_(context_.socketsMap_),
	sighandler_(evb_)
	{
		socket_ = folly::AsyncServerSocket::newSocket(evb_);
		sighandler_.registerSignalHandler(SIGINT);
	};

	void start();
    void stop();

private:
	IPCContext context_;
	folly::EventBase *evb_;
	ServerAcceptCallback scb_;
	ClientConnectionCallback ccb_;
	SysSignalHandler sighandler_;
	std::shared_ptr<folly::AsyncServerSocket> socket_;
};

}
