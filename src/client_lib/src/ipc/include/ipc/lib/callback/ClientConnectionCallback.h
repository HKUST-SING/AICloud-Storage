#pragma once

#include <folly/io/async/AsyncServerSocket.h>
#include <folly/SocketAddress.h>

#include "ipc/IPCContext.h"

namespace singaistorageipc{

class ClientConnectionCallback : public folly::AsyncServerSocket::ConnectionEventCallback{
public:
    ClientConnectionCallback() = delete;
    ClientConnectionCallback(std::shared_ptr<
          std::unordered_map<int,IPCContext::PersistentConnection>> socketsMap)
    :socketsMap_(socketsMap){};
	/**
    * onConnectionAccepted() is called right after a client connection
    * is accepted using the system accept()/accept4() APIs.
    */
    void onConnectionAccepted(const int socket,
            const folly::SocketAddress& addr) noexcept override{};
    /**
     * onConnectionAcceptError() is called when an error occurred accepting
     * a connection.
     */
    void onConnectionAcceptError(const int err) noexcept override{};
    /**
     * onConnectionDropped() is called when a connection is dropped,
     * probably because of some error encountered.
     */
    void onConnectionDropped(const int socket,
            const folly::SocketAddress& addr) noexcept override{
        LOG(WARNING) << "drop a connection";
        auto search = socketsMap_->find(socket);
        if(search != socketsMap_->end()){
            socketsMap_->erase(socket);
        }
    }; 
    /**
     * onConnectionEnqueuedForAcceptorCallback() is called when the
     * connection is successfully enqueued for an AcceptCallback to pick up.
     */
    void onConnectionEnqueuedForAcceptorCallback(
        const int socket,
        const folly::SocketAddress& addr) noexcept override{};
    /**
     * onConnectionDequeuedByAcceptorCallback() is called when the
     * connection is successfully dequeued by an AcceptCallback.
     */
    void onConnectionDequeuedByAcceptorCallback(
        const int socket,
        const folly::SocketAddress& addr) noexcept override{
        LOG(INFO) << "dequeue a connection";
        auto search = socketsMap_->find(socket);
        if(search != socketsMap_->end()){
            socketsMap_->erase(socket);
        }
    };
    /**
     * onBackoffStarted is called when the socket has successfully started
     * backing off accepting new client sockets.
     */
    void onBackoffStarted() noexcept override{};
    /**
     * onBackoffEnded is called when the backoff period has ended and the socket
     * has successfully resumed accepting new connections if there is any
     * AcceptCallback registered.
     */
    void onBackoffEnded() noexcept override{};
    /**
     * onBackoffError is called when there is an error entering backoff
     */
    void onBackoffError() noexcept override{};

private:
    std::shared_ptr<
          std::unordered_map<int,IPCContext::PersistentConnection>> socketsMap_;
};

}
