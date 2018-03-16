/*
 * This file define system signal handler.
 */

#pragma once

#include <csignal>

#include <folly/io/async/AsyncSignalHandler.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>

namespace singaistorageipc{

class SysSignalHandler : public folly::AsyncSignalHandler{
public:
	using folly::AsyncSignalHandler::AsyncSignalHandler;
	void addSocket(std::shared_ptr<folly::AsyncServerSocket> runSocket){
		runSocket_ = runSocket;
	};

	~SysSignalHandler() override{
		runSocket_ = nullptr;
	};
	
	void signalReceived(int signum) noexcept override{
		if(signum != SIGINT){
			/**
			 * We don't handle other signal here.
			 */
			return;
		}
		if(runSocket_ != nullptr){
			runSocket_->destroy();
		}
		auto evb = getEventBase();
		evb->terminateLoopSoon();
	}

private:
	std::shared_ptr<folly::AsyncServerSocket> runSocket_;
};

}
