/* Can test
 * IPC server object.
 */
#pragma once

/**
 * External dependence
 */
#include <folly/SocketAddress.h>
#include <folly/io/async/EventBase.h>
#include <folly/io/async/AsyncServerSocket.h>

/**
 * Internal dependence
 */
#include "IPCSocketPoolContext.h"

namespace ipc{

//class SignalHandler;

class IPCServer final{
public:
	/**
	 * Create a new IPC Server.
	 */
	explicit IPCServer(IPCSocketPoolContext context);
	~IPCServer();

	/**
     * Configure server to bind to the following addresses.
     *
     * Actual bind happens in `start` function.
     */
    void bind(std::vector<folly::SocketAddress>&& addrs);
    void bind(std::vector<folly::SocketAddress> const& addrs);

    /**
     * Start IPC Server.
     * 
     * Note this is a blocking call and the current thread will be used to listen
     * for incoming connections. Throws exception if something goes wrong (say
     * somebody else is already listening on that socket).
     *
     * `onSuccess` callback will be invoked from the event loop which shows that
     * all the setup was successfully done.
     *
     * `onError` callback will be invoked if some errors occurs while starting the
     * server instead of throwing exception.
	 */
    void start(std::function<void()> onSuccess = nullptr,
    		   std::function<void(std::exception_ptr)> onError = nullptr);

    /**
     * Stop listening on bound ports. (Stop accepting new work).
     * It does not wait for pending work to complete.
     * You must still invoke stop() before destroying the server.
     * You do NOT need to invoke this before calling stop().
     * This can be called from any thread, and it is idempotent.
     * However, it may only be called **after** start() has called onSuccess.
     */
	void stopListening();

	/**
     * Stop HTTPServer.
     *
   	 * Can be called from any thread, but only after start() has called
     * onSuccess. If `isHardShutdown` is false ,server will stop listening 
     * for new connections and will wait for running requests to finish.
     * Otherwise, server will stop all the requests forcedly.
     *
     */
    void stop(bool isHardShutdown = false);

    /**
     * Get the list of addresses server is listening on. Empty if sockets are not
     * bound yet.
     */
    std::vector<folly::SocketAddress> addresses() const {
    	return addresses_;
    };

    /**
     * Get the sockets the server is currently bound to.
     */
    /*
	const std::vector<const folly::AsyncSocketBase*> getSockets() const{
        return sockets_;
    };
    */
	/**
     * Returns a file descriptor associated with the listening socket
     */
    //int getListenSocket() const;

    /**
     * Do in future
     */
    /**
     * Add a socket for listening after the server has started.
     */
    //void addSocket(folly::AsyncSocketBase&& sock);
    //void addSocket(folly::AsyncSocketBase const& sock);


private:
	/**
     * Addresses we are listening on
     */
    std::vector<folly::SocketAddress> addresses_;
    /**
     * Existing sockets in server.
     */
    //std::vector<const folly::AsyncServerSocket*> sockets_;

    std::share_ptr<IPCSocketPoolContext> context_;

    /**
     * Event base in which we binded server sockets.
     */
    folly::EventBase* mainEventBase_{nullptr};

    /**
     * Optional signal handlers on which we should shutdown server
     */
//    std::unique_ptr<SignalHandler> signalHandler_;

};

}