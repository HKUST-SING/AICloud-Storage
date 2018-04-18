#pragma once

//#include <iostream>
#include <sys/mman.h>
#include <unordered_map>
#include <map>
#include <cstdlib>
#include <cstdint>

#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncSocketException.h>

#include "ServerWriteCallback.h"
#include "RequestContext.h"
#include "ipc/lib/utils/BFCAllocator.h"
#include "remote/Security.h"
#include "include/CommonCode.h"
#include "cluster/WorkerPool.h"


namespace singaistorageipc{

/*
 * This callback will be invoked when a socket can be read by server.
 */
class ServerReadCallback : public folly::AsyncReader::ReadCallback{
public:
    ServerReadCallback(size_t buf,
        const std::shared_ptr<folly::AsyncSocket>& socket,
        uint64_t minAllocBuf,
        uint64_t newAllocSize,
        uint32_t readSMSize,
        uint32_t writeSMSize,
        std::shared_ptr<Security> sec,
        std::shared_ptr<WorkerPool> worker)
    : sec_(sec),
      worker_(worker),
      bufferSize_(buf),
      minAllocBuf_(minAllocBuf),
      newAllocSize_(newAllocSize),
      readSMSize_(readSMSize),
      writeSMSize_(writeSMSize),
      readSMName_(nullptr),
      writeSMName_(nullptr),
      readSM_(nullptr),
      writeSM_(nullptr),
      socket_(socket),
      wcb_(socket.get()->getFd()){};

    /**
     * When data becomes available, getReadBuffer() will be invoked to get the
     * buffer into which data should be read.
     *
     * This method allows the ReadCallback to delay buffer allocation until
     * data becomes available.  This allows applications to manage large
     * numbers of idle connections, without having to maintain a separate read
     * buffer for each idle connection.
     *
     * It is possible that in some cases, getReadBuffer() may be called
     * multiple times before readDataAvailable() is invoked.  In this case, the
     * data will be written to the buffer returned from the most recent call to
     * readDataAvailable().  If the previous calls to readDataAvailable()
     * returned different buffers, the ReadCallback is responsible for ensuring
     * that they are not leaked.
     *
     * If getReadBuffer() throws an exception, returns a nullptr buffer, or
     * returns a 0 length, the ReadCallback will be uninstalled and its
     * readError() method will be invoked.
     *
     * getReadBuffer() is not allowed to change the transport state before it
     * returns.  (For example, it should never uninstall the read callback, or
     * set a different read callback.)
     *
     * @param bufReturn getReadBuffer() should update *bufReturn to contain the
     *                  address of the read buffer.  This parameter will never
     *                  be nullptr.
     * @param lenReturn getReadBuffer() should update *lenReturn to contain the
     *                  maximum number of bytes that may be written to the read
     *                  buffer.  This parameter will never be nullptr.
     */
    void getReadBuffer(void** bufReturn, size_t* lenReturn) override;

    bool isBufferMovable() noexcept override{
        return false;
    };

    /**
     * Suggested buffer size, allocated for read operations,
     * if callback is movable and supports folly::IOBuf
     */

    size_t maxBufferSize() const override{
        return bufferSize_;
    };

    /**
     * readBufferAvailable() will be invoked when data has been successfully
     * read.
     *
     * Note that only either readBufferAvailable() or readDataAvailable() will
     * be invoked according to the return value of isBufferMovable(). The timing
     * and aftereffect of readBufferAvailable() are the same as
     * readDataAvailable()
     *
     * @param readBuf The unique pointer of read buffer.
     */

    void readBufferAvailable(std::unique_ptr<folly::IOBuf> /*readBuf*/) 
     noexcept override{};

    /**
     * readEOF() will be invoked when the transport is closed.
     *
     * The read callback will be automatically uninstalled immediately before
     * readEOF() is invoked.
     */
    void readEOF() noexcept override;

    /**
     * readError() will be invoked if an error occurs reading from the
     * transport.
     *
     * The read callback will be automatically uninstalled immediately before
     * readError() is invoked.
     *
     * @param ex        An exception describing the error that occurred.
     */
    void readErr(const folly::AsyncSocketException& ex) noexcept override{
        readEOF();
    };

    void readDataAvailable(size_t len) noexcept override;

private:
    std::string username_;
    std::string password_;
    std::shared_ptr<Security> sec_;
    std::shared_ptr<WorkerPool> worker_;

    size_t bufferSize_;
    uint64_t minAllocBuf_;
    uint64_t newAllocSize_;
    uint32_t readSMSize_;
    uint32_t writeSMSize_;
    char* readSMName_;
    char* writeSMName_;

    void *readSM_;
    void *writeSM_;

    std::shared_ptr<folly::AsyncSocket> socket_;
    std::shared_ptr<BFCAllocator> readSMAllocator_;

    std::map<uint32_t,
        const std::unordered_map<std::string,
                        ReadRequestContext>::iterator> unallocatedRequest_;

    folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
    ServerWriteCallback wcb_;

    std::unordered_map<std::string,ReadRequestContext> readContextMap_;
    std::unordered_map<std::string,WriteRequestContext> writeContextMap_;

    /**
     * Handle function of each operation
     */
    void handleAuthenticationRequest(std::unique_ptr<folly::IOBuf> data);
    void handleReadRequest(std::unique_ptr<folly::IOBuf> data);
    void handleWriteRequest(std::unique_ptr<folly::IOBuf> data);
    void handleDeleteRequest(std::unique_ptr<folly::IOBuf> data);
    void handleCloseRequest(std::unique_ptr<folly::IOBuf> data);

    /**
     * Future callback
     */
    void callbackAuthenticationRequest(Task);
//    void callbackReadCredential(Task);
    void callbackReadRequest(Task);
    void callbackWriteCredential(Task);
    void callbackWriteRequest(Task);  
//    void callbackDeleteCredential(Task);
    void callbackDeleteRequest(Task);   

    /**
     * Helper functions
     */
    bool allocatSM(size_t &,BFCAllocator::Offset &);
    bool finishThisReadRequest(const std::string&);
    bool passReadRequesttoTask(uint32_t, const std::unordered_map
				<std::string,ReadRequestContext>::iterator);
    bool doReadCredential(uint32_t,const std::string&);
    void sendWriteReply(std::string,uint32_t,uint32_t);
    void doWriteRequestCallback(Task,uint32_t);

    std::unordered_map<uint32_t,folly::Future<Task>> futurePool_;

    void sendStatus(uint32_t id, CommonCode::IOStatus type);

    /**
     * Const default value
     */
    const size_t defaultAllocSize_{std::numeric_limits<uint16_t>::max()};
};

}
