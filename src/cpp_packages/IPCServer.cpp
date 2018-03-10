/* Can test
 * IPC server object.
 */
#pragma once

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
#include "lib/Callback.h"

namespace singaistorageipc{

void IPCServer::start(){
    auto evb = folly::EventBaseManager::get()->getEventBase();
    auto socket = folly::AsyncServerSocket::newSocket(std::move(evb));
    folly::SocketAddress addr = folly::SocketAddress::makeFromPath("server_socket");
    socket->bind(addr);
    ServerAcceptCallback cb;
    socket->addAcceptCallback(&cb,std::move(evb));
    socket->listen(10);
    socket->startAccepting();
    std::cout << "server starting......" << std::endl;
    evb->loopForever();
};

}
