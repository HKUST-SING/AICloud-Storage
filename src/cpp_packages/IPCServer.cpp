/*
 * Can test
 */

#pragma once

#include <iostream>

#include <folly/io/async/EventBaseManager.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/system/ThreadName.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/executors/ThreadPoolExecutor.h>

#include "lib/IPCServer.h"

using folly::EventBaseManager;
using folly::IOThreadPoolExecutor;
using folly::ThreadPoolExecutor;
using folly::AsyncServerSocket;
using folly::SocketAddress;

namespace ipc{

IPCServer::IPCServer(IPCSocketPoolContext context){
	this->context_ = std::make_shared<IPCSocketPoolContext>(std::move(context));
}

void IPCServer::bind(std::vector<folly::SocketAddress>&& addrs) {
  addresses_ = std::move(addrs);
}

void IPCServer::bind(std::vector<folly::SocketAddress> const& addrs) {
  addresses_ = addrs;
}

class HandlerCallbacks : public folly::ThreadPoolExecutor::Observer {
public:
	explicit HandlerCallbacks(std::shared_ptr<IPCSocketPoolContext> context) : context_(context) {}

	void threadStarted(ThreadPoolExecutor::ThreadHandle* h) override {
    	auto evb = IOThreadPoolExecutor::getEventBase(h);
    	CHECK(evb) << "Invariant violated - started thread must have an EventBase";
    	evb->runInEventBaseThread([=](){
      		/*
		for (auto& factory: context_->handlerFactories) {
        		factory->onServerStart(evb);
      		}
		*/
    	});
	}
	void threadStopped(ThreadPoolExecutor::ThreadHandle* h) override {
    	IOThreadPoolExecutor::getEventBase(h)->runInEventBaseThread([&](){
    		/*
		for (auto& factory: context_->handlerFactories) {
        		factory->onServerStop();
      		}
		*/
    	});
  	}
private:
	std::shared_ptr<IPCSocketPoolContext> context_;
};

class AcceptCallback : public AsyncServerSocket::AcceptCallback{
public:
	/**
     * connectionAccepted() is called whenever a new client connection is
     * received.
     *
     * The AcceptCallback will remain installed after connectionAccepted()
     * returns.
     *
     * @param fd          The newly accepted client socket.  The AcceptCallback
     *                    assumes ownership of this socket, and is responsible
     *                    for closing it when done.  The newly accepted file
     *                    descriptor will have already been put into
     *                    non-blocking mode.
     * @param clientAddr  A reference to a SocketAddress struct containing the
     *                    client's address.  This struct is only guaranteed to
     *                    remain valid until connectionAccepted() returns.
     */
	void connectionAccepted(int fd,const SocketAddress& clientAddr)
	noexcept override{
		std::cout << "fd: " << fd << "\n";
	};

	/**
     * acceptError() is called if an error occurs while accepting.
     *
     * The AcceptCallback will remain installed even after an accept error,
     * as the errors are typically somewhat transient, such as being out of
     * file descriptors.  The server socket must be explicitly stopped if you
     * wish to stop accepting after an error.
     *
     * @param ex  An exception representing the error.
     */
	void acceptError(const std::exception& ex) noexcept override{

	};

	/**
     * acceptStarted() will be called in the callback's EventBase thread
     * after this callback has been added to the AsyncServerSocket.
     *
     * acceptStarted() will be called before any calls to connectionAccepted()
     * or acceptError() are made on this callback.
     *
     * acceptStarted() makes it easier for callbacks to perform initialization
     * inside the callback thread.  (The call to addAcceptCallback() must
     * always be made from the AsyncServerSocket's primary EventBase thread.
     * acceptStarted() provides a hook that will always be invoked in the
     * callback's thread.)
     *
     * Note that the call to acceptStarted() is made once the callback is
     * added, regardless of whether or not the AsyncServerSocket is actually
     * accepting at the moment.  acceptStarted() will be called even if the
     * AsyncServerSocket is paused when the callback is added (including if
     * the initial call to startAccepting() on the AsyncServerSocket has not
     * been made yet).
     */
	void acceptStarted() noexcept override{

	};

	/**
     * acceptStopped() will be called when this AcceptCallback is removed from
     * the AsyncServerSocket, or when the AsyncServerSocket is destroyed,
     * whichever occurs first.
     *
     * No more calls to connectionAccepted() or acceptError() will be made
     * after acceptStopped() is invoked.
     */
	void acceptStopped() noexcept override{

	};
};


void IPCServer::start(std::function<void()> onSuccess,
		   std::function<void(std::exception_ptr)> onError){
	mainEventBase_ = EventBaseManager::get()->getEventBase();
	auto listen_pool = std::make_shared<IOThreadPoolExecutor>(context_->listener_threads,
		std::make_shared<folly::NamedThreadFactory>("IPCServer"));
	auto exeObserver = std::make_shared<HandlerCallbacks>(context_);
	listen_pool->addObserver(exeObserver);

	try{
		for(auto i = addresses_.begin();i!=addresses_.end();i++){
			if(i->getFamily() != AF_UNIX){
				addresses_.erase(i);
				i--;
				continue;
			}
			auto serverSocket = AsyncServerSocket::newSocket(mainEventBase_);
			serverSocket->bind(*i);
			// TODO: build a acceptable function
            AcceptCallback a;
			serverSocket->addAcceptCallback(&a,mainEventBase_);
            serverSocket->listen(context_->listenBacklog);

			auto f = [&](){
				serverSocket->startAccepting();
			};
			listen_pool->add(f);
		}
	}catch (const std::exception& ex) {
    	stop();

    	if (onError) {
      		onError(std::current_exception());
      		return;
    	}

    	throw;
	}

	// TODO:Install signal handler

	mainEventBase_->loopForever();
}


}

