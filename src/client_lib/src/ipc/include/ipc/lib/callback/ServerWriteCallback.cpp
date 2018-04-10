

#include <iostream>

#include <folly/io/async/AsyncSocketException.h>

#include "ServerWriteCallback.h"

namespace singaistorageipc{

void ServerWriteCallback::writeSuccess() noexcept{
    	DLOG(INFO) << fd_ << ": write successfully";
};

void ServerWriteCallback::writeErr(size_t bytesWritten, 
    	const folly::AsyncSocketException& ex) noexcept{
    	LOG(WARNING) << fd_ << ": error in write\n ERROR TYPE: "<< ex.getType();
};

}