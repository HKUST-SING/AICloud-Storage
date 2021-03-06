

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>

#include "ipc/lib/callback/ServerAcceptCallback.h"
#include "ipc/lib/callback/ServerReadCallback.h"

namespace singaistorageipc{

void ServerAcceptCallback::connectionAccepted(int fd,
	const folly::SocketAddress& clientAddr)noexcept{
	auto s = folly::AsyncSocket::newSocket(evb_,fd);
	auto cb = std::shared_ptr<ServerReadCallback>(
		new ServerReadCallback(bufferSize_,s,minAllocBuf_,newAllocSize_,
			readsm_,writesm_,sec_,worker_,evb_));
    s->setReadCB(cb.get());
    socketsMap_->insert({fd,{s,cb}});
};

void ServerAcceptCallback::acceptStopped() noexcept{
    DLOG(INFO) << "start to stop accepting";
    for(auto socket : (*socketsMap_)){
    	socket.second.socket_->closeNow();
    }
    socketsMap_->clear();
    std::remove(unixPath_.c_str());
    socketsMap_ = nullptr;
    sec_ = nullptr;
    DLOG(INFO) << "stopped accepting";
};

}
