#pragma once


#include <folly/io/async/AsyncServerSocket.h>
#include <folly/SocketAddress.h>

#include "ServerReadCallback.h"

namespace singaistorageipc{

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


}