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

	~SysSignalHandler() override{
	};
	
	void signalReceived(int signum) noexcept override{
		if(signum != SIGINT){
			/**
			 * We don't handle other signal here.
			 */
			return;
		}
		LOG(INFO) << "receive SIGINT signal, processing termination";
		auto evb = getEventBase();
		evb->terminateLoopSoon();
	}
};

}
