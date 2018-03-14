#pragma once

//#include <iostream>
#include <sys/mman.h>
#include <unordered_map>
#include <queue>
#include <ctime>
#include <cstdlib>

#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncSocketException.h>

#include "ServerWriteCallback.h"
#include "RequestContext.h"
#include "../message/IPCReadRequestMessage.h"
#include "../message/IPCWriteRequestMessage.h"
#include "../utils/BFCAllocator.h"


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
        uint32_t writeSMSize)
    : bufferSize_(buf),
      socket_(socket),
      wcb_(socket.get()->getFd()),
      minAllocBuf_(minAllocBuf),
      newAllocSize_(newAllocSize),
      readSMSize_(readSMSize),
      writeSMSize_(writeSMSize),
      readSM_(nullptr),
      writeSM_(nullptr){std::srand(time(NULL));};

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

    size_t bufferSize_;
    uint64_t minAllocBuf_;
    uint64_t newAllocSize_;
    uint32_t readSMSize_;
    uint32_t writeSMSize_;
    char readSMName_[32];
    char writeSMName_[32];

    void *readSM_;
    void *writeSM_;

    std::shared_ptr<folly::AsyncSocket> socket_;
    std::shared_ptr<BFCAllocator> readSMAllocator_;
//    std::shared_ptr<BFCAllocator> writeSMAllocator_;

    folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
    ServerWriteCallback wcb_;

    std::unordered_map<std::string,
        std::shared_ptr<ReadRequestContext>> readContextMap_;
    std::unordered_map<std::string,
        std::shared_ptr<WriteRequestContext>> writeContextMap_;
/*
    std::unordered_map<std::string,
        std::queue<IPCReadRequestMessage>> readRequest_;
    std::unordered_map<std::string,
        std::queue<IPCWriteRequestMessage>> writeRequest_;

    std::unordered_map<std::string,IPCWriteRequestMessage> lastReadResponse_;
    std::unordered_map<std::string,IPCReadRequestMessage> lastWriteResponse_;
*/

    void handleAuthenticationRequest(std::unique_ptr<folly::IOBuf> data);
    void handleReadRequest(std::unique_ptr<folly::IOBuf> data);
    void handleWriteRequest(std::unique_ptr<folly::IOBuf> data);
    void handleDeleteRequest(std::unique_ptr<folly::IOBuf> data);
    void handleCloseRequest();

};

}
