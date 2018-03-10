#pragma once

#include <iostream>

#include "lib/Callback.h"


namespace singaistorageipc{

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn){
	auto res = readBuffer_.preallocate(1, 5);
    *bufReturn = res.first;
    *lenReturn = res.second;	
};

void ServerReadCallback::readBufferAvailable(std::unique_ptr<folly::IOBuf> readBuf){
	std::cout << socket_->getFd() << ":" 
		<< readBuf->data() << ":" << readBuf->length() << std::endl;
	readBuffer_.append(std::move(readBuf));	
};

void ServerWriteCallback::writeSuccess() noexcept{
    	std::cout << fd_ << ":write successfully" << std::endl;
};

void ServerWriteCallback::writeErr(size_t bytesWritten, 
    	const folly::AsyncSocketException& ex) noexcept{
    	std::cout << fd_ << ":error in write" << std::endl;
};

}
