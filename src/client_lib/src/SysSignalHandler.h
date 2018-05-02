/*
 * This file define system signal handler.
 */

#pragma once

#include <csignal>

// Facebook folly
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
		auto evb = getEventBase();
		evb->terminateLoopSoon();

		// Stop each module.
		// The order reverts from the initialization.
		ipcserver_->stop();
	}
private:
	std::unique_ptr<IPCServer> ipcserver_;
};

}
