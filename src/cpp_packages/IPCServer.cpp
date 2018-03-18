/* Can test
 * IPC server object.
 */

#include <iostream>
#include <csignal>

/**
 * External dependence
 */
#include <folly/SocketAddress.h> 

/**
 * Internal dependence
 */
#include "lib/callback/ServerAcceptCallback.h"
#include "lib/callback/ClientConnectionCallback.h"
#include "IPCServer.h"
#include "SysSignalHandler.h"

namespace singaistorageipc{

void IPCServer::start(){
    socket_->bind(context_.addr_);

    ServerAcceptCallback scb(
    	context_.bufferSize_,context_.minAllocBuf_,
	   context_.newAllocSize_,context_.readSMSize_,
        context_.writeSMSize_,context_.addr_.getPath(),
        context_.socketsMap_);
    socket_->addAcceptCallback(&scb,evb);

    ClientConnectionCallback ccb(context_.socketsMap_);
    socket_->setConnectionEventCallback(&ccb);
    
    SysSignalHandler sighandler{evb_};
    sighandler.registerSignalHandler(SIGINT);

    socket_->listen(context_.backlog_);
    socket_->startAccepting();
    
    std::cout << "server starting......" << std::endl;
    evb->loopForever();
    /**
     * TODO: seagmentation fault will be threw if we use return here.
     */
    //exit(0);
}

void IPCServer::stop(){
    std::cout << "server stopping......" << std::endl;
    socket_->destroy();
    evb_->loopOnce();
    std::cout << "server stop" << std::endl;
}

}
