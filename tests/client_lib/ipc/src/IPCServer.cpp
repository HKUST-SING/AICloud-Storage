/*
 * IPC server object.
 */

#include <iostream>

/**
 * External dependence
 */
#include <folly/SocketAddress.h> 

/**
 * Internal dependence
 */
#include "ipc/IPCServer.h"

namespace singaistorageipc{

void IPCServer::start(){
    socket_->bind(context_.addr_);

    socket_->addAcceptCallback(&scb_,evb_);

    socket_->setConnectionEventCallback(&ccb_);
    
//    SysSignalHandler sighandler{evb_};
    

    socket_->listen(context_.backlog_);
    socket_->startAccepting();
    
    LOG(INFO) << "server starting......";
    evb_->loopForever();

}

void IPCServer::stop(){
    LOG(INFO) << "server stopping......";
    socket_->stopAccepting();
    socket_ = nullptr;
    evb_->loopOnce();
    LOG(INFO) << "server stopped";
}

}
