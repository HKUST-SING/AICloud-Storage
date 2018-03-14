/*
 * IPCContext includes the argument needed in IPCServer
 */
#pragma once

#include <folly/SocketAddress.h> 

namespace singaistorageipc{

class IPCContext{
public:
	IPCContext(std::string s, int backlog){
		addr_ = folly::SocketAddress::makeFromPath(s);
		backlog_ = backlog;
	};

	folly::SocketAddress addr_;

	int backlog_;

	/**
     * Suggested buffer size, allocated for read operations,
     * if callback is movable and supports folly::IOBuf
     */
	size_t bufferSize_{1024};
	/**
	 * The amount of space apply for receiving buffer
	 * will be at least minAllocBuf_.
	 */
	uint64_t minAllocBuf_{64};
	/**
	 * If min contiguous space is not available at the 
   	 * end of the queue, and IOBuf with size newAllocSize_
     * is appended to the chain and returned.
	 */
	uint64_t newAllocSize_{1024};

	uint32_t readSMSize_{1000*1000*1000}; // 1G
	uint32_t writeSMSize_{1000*1000*1000};
};

}