/*
 * This head file contains all callback we need to use.
 */

#pragma once

#include <iostream>

#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocketException.h>
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <folly/SocketAddress.h>

namespace singaistorageipc{

class ClientConnectionCallback : public folly::AsyncServerSocket::ConnectionEventCallback{
public:
    void onConnectionAccepted(const int socket,
            const folly::SocketAddress& addr) noexcept override{};
    void onConnectionAcceptError(const int err) noexcept override{};
    void onConnectionDropped(const int socket,
            const folly::SocketAddress& addr) noexcept override{}; 
    void onConnectionEnqueuedForAcceptorCallback(
        const int socket,
        const folly::SocketAddress& addr) noexcept override{};
    void onConnectionDequeuedByAcceptorCallback(
        const int socket,
        const folly::SocketAddress& addr) noexcept override{};
    void onBackoffStarted() noexcept override{};
    void onBackoffEnded() noexcept override{};
    void onBackoffError() noexcept override{};
};

/*
 * This callback will be invoked when a socket can be read by server.
 */
class ServerReadCallback : public folly::AsyncReader::ReadCallback{
public:
    // TODO: User need set buffer size when create this callback
    ServerReadCallback(size_t buf,
        const std::shared_ptr<folly::AsyncSocket>& socket)
    : buffersize_(buf),
      socket_(socket){};

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
        return buffersize_;
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
    size_t buffersize_;
    std::shared_ptr<folly::AsyncSocket> socket_;
    folly::IOBufQueue readBuffer_{folly::IOBufQueue::cacheChainLength()};
};


/*
 *This callback will be invoked when a connection event happen.
 */
class ServerAcceptCallback : public folly::AsyncServerSocket::AcceptCallback{
public:
	/**
     * connectionAccepted() is called whenever a new client connection is
     * received.
     *
     * The AcceptCallback will remain installed after connectionAccepted()
     * returns.
     *
     * @param fd          The newly accepted client socket.  The AcceptCallback
     *                    assumes ownership of this socket, and is responsible
     *                    for closing it when done.  The newly accepted file
     *                    descriptor will have already been put into
     *                    non-blocking mode.
     * @param clientAddr  A reference to a SocketAddress struct containing the
     *                    client's address.  This struct is only guaranteed to
     *                    remain valid until connectionAccepted() returns.
     */
	void connectionAccepted(int fd,const folly::SocketAddress& clientAddr)
	noexcept override;

	/**
     * acceptError() is called if an error occurs while accepting.
     *
     * The AcceptCallback will remain installed even after an accept error,
     * as the errors are typically somewhat transient, such as being out of
     * file descriptors.  The server socket must be explicitly stopped if you
     * wish to stop accepting after an error.
     *
     * @param ex  An exception representing the error.
     */
	void acceptError(const std::exception& ex) noexcept override{

	};

	/**
     * acceptStarted() will be called in the callback's EventBase thread
     * after this callback has been added to the AsyncServerSocket.
     *
     * acceptStarted() will be called before any calls to connectionAccepted()
     * or acceptError() are made on this callback.
     *
     * acceptStarted() makes it easier for callbacks to perform initialization
     * inside the callback thread.  (The call to addAcceptCallback() must
     * always be made from the AsyncServerSocket's primary EventBase thread.
     * acceptStarted() provides a hook that will always be invoked in the
     * callback's thread.)
     *
     * Note that the call to acceptStarted() is made once the callback is
     * added, regardless of whether or not the AsyncServerSocket is actually
     * accepting at the moment.  acceptStarted() will be called even if the
     * AsyncServerSocket is paused when the callback is added (including if
     * the initial call to startAccepting() on the AsyncServerSocket has not
     * been made yet).
     */
	void acceptStarted() noexcept override{

	};

	/**
     * acceptStopped() will be called when this AcceptCallback is removed from
     * the AsyncServerSocket, or when the AsyncServerSocket is destroyed,
     * whichever occurs first.
     *
     * No more calls to connectionAccepted() or acceptError() will be made
     * after acceptStopped() is invoked.
     */
	void acceptStopped() noexcept override{
        sockets_pool_.clear();
	};

private:
    std::vector<std::shared_ptr<folly::AsyncSocket>> sockets_pool_;
    std::vector<std::shared_ptr<ServerReadCallback>> readcallbacks_pool_;
};


class ServerWriteCallback : public folly::AsyncWriter::WriteCallback{
public:
	ServerWriteCallback(int fd):fd_(fd){};
	/**
     * writeSuccess() will be invoked when all of the data has been
     * successfully written.
     *
     * Note that this mainly signals that the buffer containing the data to
     * write is no longer needed and may be freed or re-used.  It does not
     * guarantee that the data has been fully transmitted to the remote
     * endpoint.  For example, on socket-based transports, writeSuccess() only
     * indicates that the data has been given to the kernel for eventual
     * transmission.
     */
    void writeSuccess() noexcept override;

    /**
     * writeError() will be invoked if an error occurs writing the data.
     *
     * @param bytesWritten      The number of bytes that were successfull
     * @param ex                An exception describing the error that occurred.
     */
    void writeErr(size_t bytesWritten, 
    	const folly::AsyncSocketException& ex) noexcept override;
private:
	int fd_;
};

}
