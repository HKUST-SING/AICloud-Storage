#pragma once

#include <iostream>

#include "lib/callback/ClientConnectionCallback.h"
#include "lib/callback/ServerAcceptCallback.h"
#include "lib/callback/ServerReadCallback.h"
#include "lib/callback/ServerWriteCallback.h"

#include <folly/io/IOBufQueue.h>
#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/AsyncSocketException.h>


namespace singaistorageipc{

void ServerAcceptCallback::connectionAccepted(int fd,const folly::SocketAddress& clientAddr)noexcept{
	auto s = folly::AsyncSocket::newSocket(folly::EventBaseManager::get()->getEventBase(),fd);
//        ServerReadCallback cb(10,s);
	auto cb = std::shared_ptr<ServerReadCallback>(new ServerReadCallback(1000,s));
        s->setReadCB(cb.get());
    sockets_pool_.emplace_back(s);
    readcallbacks_pool_.emplace_back(cb);
};

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn){
	auto res = readBuffer_.preallocate(100, 500);
    *bufReturn = res.first;
    *lenReturn = res.second;	
};

void ServerReadCallback::readDataAvailable(size_t len)noexcept{
	readBuffer_.postallocate(len);
	std::cout << "read data available:"<< isBufferMovable() << std::endl;
	auto buf = std::move(readBuffer_.pop_front());
	std::cout << socket_->getFd() << ":" 
			<< buf->data() 
			<< ":" << buf->length()
			<< std::endl; 
	//socket_->writeChain(&wcb,std::move(buf));
};

void ServerReadCallback::readBufferAvailable(
	std::unique_ptr<folly::IOBuf> readBuf)noexcept{
	std::cout << socket_->getFd() << ":" 
		<< readBuf->data() << ":" << readBuf->length() << std::endl;
	readBuffer_.append(std::move(readBuf));	
};

void ServerWriteCallback::writeSuccess() noexcept{
    	std::cout << fd_ << ":write successfully" << std::endl;
};

void ServerWriteCallback::writeErr(size_t bytesWritten, 
    	const folly::AsyncSocketException& ex) noexcept{
    	std::cout << fd_ << ":error in write:"<< ex.getType() << std::endl;
};

}
