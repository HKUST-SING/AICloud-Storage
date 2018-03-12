#pragma once

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>

#include "ServerAcceptCallback.h"
#include "ServerReadCallback.h"

namespace singaistorageipc{

void ServerAcceptCallback::connectionAccepted(int fd,const folly::SocketAddress& clientAddr)noexcept{
	auto s = folly::AsyncSocket::newSocket(folly::EventBaseManager::get()->getEventBase(),fd);
	auto cb = std::shared_ptr<ServerReadCallback>(new ServerReadCallback(1000,s));
        s->setReadCB(cb.get());
    socketsPool_.emplace_back(s);
    readcallbacksPool_.emplace_back(cb);
};

}