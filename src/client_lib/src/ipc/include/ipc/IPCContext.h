/*
 * IPCContext includes the argument needed in IPCServer
 */
#pragma once

#include <folly/SocketAddress.h> 
#include <folly/io/async/AsyncSocket.h>

#include "lib/callback/ServerReadCallback.h"
#include "remote/Security.h"

namespace singaistorageipc{

class IPCContext{
public:
	IPCContext(std::string s, int backlog){
		addr_ = folly::SocketAddress::makeFromPath(s);
		backlog_ = backlog;
		socketsMap_ = std::make_shared<
						std::unordered_map<int,PersistentConnection>>();
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

	uint32_t readSMSize_{1000*1000}; // 1M
	uint32_t writeSMSize_{1000*1000};

	class PersistentConnection{
    public:
        PersistentConnection(
            std::shared_ptr<folly::AsyncSocket> socket,
            std::shared_ptr<ServerReadCallback> readcallback):
            socket_(socket),readcallback_(readcallback){};
        ~PersistentConnection(){
            socket_ = nullptr;
            readcallback_ = nullptr;
        }
        std::shared_ptr<folly::AsyncSocket> socket_;
        std::shared_ptr<ServerReadCallback> readcallback_;
    };

    std::shared_ptr<
    	std::unordered_map<int,PersistentConnection>> socketsMap_;

	std::shared_ptr<Security> sec_{nullptr};    
};

}
