#pragma once

#include <iostream>

#include <folly/io/async/AsyncSocketException.h>

#include <lib/callback/ServerWriteCallback.h>

namespace singaistorageipc{

void ServerWriteCallback::writeSuccess() noexcept{
    	std::cout << fd_ << ":write successfully" << std::endl;
};

void ServerWriteCallback::writeErr(size_t bytesWritten, 
    	const folly::AsyncSocketException& ex) noexcept{
    	std::cout << fd_ << ":error in write:"<< ex.getType() << std::endl;
};

}