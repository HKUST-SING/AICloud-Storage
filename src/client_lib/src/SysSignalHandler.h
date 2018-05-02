/*
 * This file define system signal handler.
 */

#pragma once

#include <csignal>

#include <folly/io/async/AsyncSignalHandler.h>
#include <folly/io/async/EventBase.h>

namespace singaistorageipc{

class SysSignalHandler : public folly::AsyncSignalHandler{
public:
	using folly::AsyncSignalHandler::AsyncSignalHandler;

	~SysSignalHandler() override{
	};

	int runProcess(int argc, const char **argv);
	
	void signalReceived(int signum) noexcept override{
		LOG(INFO) << "receive signal";
		if(signum != SIGINT){
			/**
			 * We don't handle other signal here.
			 */
			return;
		}
		auto evb = getEventBase();
		evb->terminateLoopSoon();
	}
};

}
