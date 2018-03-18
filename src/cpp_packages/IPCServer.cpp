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
#include "IPCServer.h"

namespace singaistorageipc{

void IPCServer::start(){
    socket_->bind(context_.addr_);

/*    ServerAcceptCallback scb(
    	context_.bufferSize_,context_.minAllocBuf_,
	   context_.newAllocSize_,context_.readSMSize_,
        context_.writeSMSize_,context_.addr_.getPath(),
        context_.socketsMap_);
*/    socket_->addAcceptCallback(&scb_,evb_);

//    ClientConnectionCallback ccb(context_.socketsMap_);
    socket_->setConnectionEventCallback(&ccb_);
    
//    SysSignalHandler sighandler{evb_};
    

    socket_->listen(context_.backlog_);
    socket_->startAccepting();
    
    std::cout << "server starting......" << std::endl;
    evb_->loopForever();
    /**
     * TODO: seagmentation fault will be threw if we use return here.
     */
    //exit(0);
}

void IPCServer::stop(){
    std::cout << "server stopping......" << std::endl;
    socket_->stopAccepting();
    socket_ = nullptr;
    evb_->loopOnce();
    std::cout << "server stop" << std::endl;
}

}
