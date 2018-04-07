/* Can test
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
    
    std::cout << "server starting......" << std::endl;
    evb_->loopForever();

}

void IPCServer::stop(){
    std::cout << "server stopping......" << std::endl;
    socket_->stopAccepting();
    socket_ = nullptr;
    evb_->loopOnce();
    std::cout << "server stop" << std::endl;
}

}
