#pragma once

#include <folly/io/IOBufQueue.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>

#include <lib/callback/ServerReadCallback.h>
#include <lib/callback/ServerWriteCallback.h>

namespace singaistorageipc{

IPCMessage ServerReadCallback::getMessage(){
	
};

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn){
	auto res = readBuffer_.preallocate(100, 500);
    *bufReturn = res.first;
    *lenReturn = res.second;	
};

void ServerReadCallback::readDataAvailable(size_t len)noexcept{
	readBuffer_.postallocate(len);
	std::cout << "read data available:"<< isBufferMovable() << std::endl;
	/**
	 * Get the message, now it still std::unique_ptr<folly::IOBuf>
	 */
	auto data = std::move(readBuffer_.pop_front());
/*	std::cout << socket_->getFd() << ":" 
			<< data->data() 
			<< ":" << data->length()
			<< std::endl; 
*/	
	//socket_->writeChain(&wcb,std::move(data));
	
};

}