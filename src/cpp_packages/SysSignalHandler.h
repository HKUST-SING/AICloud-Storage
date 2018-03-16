/*
 * This file define system signal handler.
 */

#pragma once

#include <csignal>

#include <folly/io/async/AsyncSignalHandler.h>

namespace singaistorageipc{

class SysSignalHandler : public folly::AsyncSignalHandler{
public:
	SysSignalHandler(EventBase* eventBase,
		std::shared_ptr<AsyncServerSocket> runSocket)
		:eventBase_(eventBase),
		 runSocket_(runSocket){};

	~SysSignalHandler() override{
		eventBase_ = nullptr;
		runSocket_ = nullptr;
	};
	
	void signalReceived(int signum) noexcept override{
		if(signum != std::SIGINT){
			/**
			 * We don't handle other signal here.
			 */
			return;
		}
		runSocket_->destroy();
		eventBase_->terminateLoopSoon();
	}

private:
	std::shared_ptr<AsyncServerSocket> runSocket_;
};

}