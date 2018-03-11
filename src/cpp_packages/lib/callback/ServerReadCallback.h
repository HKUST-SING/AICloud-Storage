#pragma once

#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/io/async/AsyncSocketException.h>

#include "ServerWriteCallback.h"


namespace singaistorageipc{

/*
 * This callback will be invoked when a socket can be read by server.
 */
class ServerReadCallback : public folly::AsyncReader::ReadCallback{
public:
    // TODO: User need set buffer size when create this callback
    ServerReadCallback(size_t buf,
        const std::shared_ptr<folly::AsyncSocket>& socket)
    : bufferSize_(buf),
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
        return true;
    };

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

    void readBufferAvailable(std::unique_ptr<folly::IOBuf> /*readBuf*/) noexcept override;

    /**
     * readEOF() will be invoked when the transport is closed.
     *
     * The read callback will be automatically uninstalled immediately before
     * readEOF() is invoked.
     */
    //virtual void readEOF() noexcept；

     /**
     * readError() will be invoked if an error occurs reading from the
     * transport.
     *
     * The read callback will be automatically uninstalled immediately before
     * readError() is invoked.
     *
     * @param ex        An exception describing the error that occurred.
     */
    void readErr(const folly::AsyncSocketException& ex) noexcept override{};

    void readDataAvailable(size_t len) noexcept override;

    void readEOF() noexcept override{};

private:
    size_t bufferSize_;
    std::shared_ptr<folly::AsyncSocket> socket_;
    folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
    ServerWriteCallback wcb_;
};

}