/* Can test
 * IPC server object.
 */

#include <iostream>

/**
 * External dependence
 */
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/SocketAddress.h> 

/**
 * Internal dependence
 */
#include "lib/callback/ServerAcceptCallback.h"
#include "lib/callback/ClientConnectionCallback.h"
#include "IPCServer.h"

namespace singaistorageipc{

void IPCServer::start(){
    auto evb = folly::EventBaseManager::get()->getEventBase();
    auto socket = folly::AsyncServerSocket::newSocket(std::move(evb));
    socket->bind(context_.addr_);

    ServerAcceptCallback scb(
    	context_.bufferSize_,context_.minAllocBuf_,
	context_.newAllocSize_,context_.readSMSize_,context_.writeSMSize_);
    socket->addAcceptCallback(&scb,std::move(evb));

    ClientConnectionCallback ccb;
    socket->setConnectionEventCallback(&ccb);
    
    socket->listen(context_.backlog_);
    socket->startAccepting();
    std::cout << "server starting......" << std::endl;
    evb->loopForever();
};

}
