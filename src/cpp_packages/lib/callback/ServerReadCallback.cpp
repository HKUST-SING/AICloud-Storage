
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <folly/io/IOBufQueue.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>

#include "ServerReadCallback.h"
#include "ServerWriteCallback.h"
#include "../message/Task.h"
#include "../message/IPCMessage.h"
#include "../message/IPCAuthenticationMessage.h"
#include "../message/IPCConnectionReplyMessage.h"
#include "../message/IPCReadRequestMessage.h"
#include "../message/IPCWriteRequestMessage.h"
#include "../message/IPCStatusMessage.h"
#include "../utils/Hash.h"
#include "../utils/BFCAllocator.h"

namespace singaistorageipc{

bool ServerReadCallback::insertReadRequest(
	IPCReadRequestMessage msg){
	auto search = readRequest_.find(msg.getPath());
	if(search == readRequest_.end()){
		std::queue<IPCReadRequestMessage> queue;
		queue.emplace(msg);
		readRequest_.emplace(msg.getPath(),queue);
		return true;
	}
	else{
		search->second.emplace(msg);
		return false;
	}
}

bool ServerReadCallback::insertWriteRequest(
	IPCWriteRequestMessage msg){
	auto search = writeRequest_.find(msg.getPath());
	if(search == writeRequest_.end()){
		std::queue<IPCWriteRequestMessage> queue;
		queue.emplace(msg);
		writeRequest_.emplace(msg.getPath(),queue);
		return true;
	}
	else{
		search->second.emplace(msg);
		return false;
	}
}

void ServerReadCallback::handleAuthticationRequest(
	std::unique_ptr<folly::IOBuf> data){

	if(readSM_ != nullptr || writeSM_ != nullptr){
		/**
		 * The user has authenticated before.
		 */

		// Or we can send a STATUS message here.
		return ;
	}

	IPCAuthenticationMessage auth_msg;
	// If parse fail, stop processing.
	if(!auth_msg.parse(std::move(data))){
		return;
	}

	/**
	 * TODO: Send the user info to the authentication server.
	 *
	 * Here we only grant each authentication.
	 */
	std::string username = auth_msg.getUsername();
	char pw[32];
	auth_msg.get(pw);

	/**
	 * TODO: Check result and send back to the client.
	 *
	 * Now we just send success.
	 */
	username_ = username;

	/**
	 * Get share memory name.
	 */
	time_t t;
	t = time(NULL);
	char *readSMName = memName32(ctime(&t));
	t = time(NULL);
	char *writeSMName = memName32(ctime(&t));

	/**
	 * Create share memory.
	 */
	int readfd;
	while((readfd = shm_open(readSMName,
		O_CREAT|O_EXCL|O_RDWR,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) == EEXIST){
		/*
		 * The object already exists. Change another name.
		 */
		t = time(NULL);
		readSMName = memName32(ctime(&t));
	}
	memcpy(readSMName_,readSMName,32*sizeof(char));

	int writefd;
	while((writefd = shm_open(writeSMName,
		O_CREAT|O_EXCL|O_RDWR,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) == EEXIST){
		/*
		 * The object already exists. Change another name.
		 */
		t = time(NULL);
		writeSMName = memName32(ctime(&t));
	}
	memcpy(writeSMName_,writeSMName,32*sizeof(char));

	/**
	 * Set the size of share memory.
	 */
	ftruncate(readfd,readSMSize_);
	ftruncate(writefd,writeSMSize_);

	/**
	 * Map the share memory to the virtual address.
	 */
	readSM_ = mmap(NULL, readSMSize_, PROT_READ | PROT_WRITE,
					MAP_SHARED, readfd, 0);
	close(readfd);
	writeSM_ = mmap(NULL, writeSMSize_, PROT_READ | PROT_WRITE,
					MAP_SHARED, writefd, 0);
	close(writefd);

	/**
	 * Register allocater for share memory.
	 */
	readSMAllocator_ = BFCAllocator::create(0,readSMSize_);
	writeSMAllocator_ = BFCAllocator::create(1,writeSMSize_);

	/**
	 * Send the reply.
	 */
	IPCConnectionReplyMessage reply;
	reply.setWriteBufferAddress((uint64_t)writeSM_);
	reply.setReadBufferAddress((uint64_t)readSM_);
	reply.setWriteBufferSize(writeSMSize_);
	reply.setReadBufferSize(readSMSize_);
	reply.setWriteBufferName(writeSMName_);
	reply.setReadBufferName(readSMName_);

	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf));
};

void ServerReadCallback::handleReadRequest(
	std::unique_ptr<folly::IOBuf> data){
	IPCReadRequestMessage read_msg;
	// If parse fail, stop processing.
	if(!read_msg.parse(std::move(data))){
		return;
	}

	/**
	 * TODO: check the bitmap.
	 */
	// Whether this read request is an new request.
	uint32_t pro = read_msg.getProperties();
	bool isnewcoming;


	std::string path = read_msg.getPath();
	/**
	 * TODO: store this info.
	 */
	uint32_t workID = 0;
	uint32_t tranID = 0;
	uint32_t objectSize = 1024;

	if(isnewcoming){
		/**
	 	 * TODO: check user credentials and correctness of operation.
	 	 *
	 	 * Here we only grant each operation.
	 	 */

		if(!insertReadRequest(read_msg)){
			// There is other read request processing
			// of the same object.
			return ;
		}
	}
	else{
		// It is a comfirm.
		auto kv_pair = lastReadResponse_.find(path);

		if(kv_pair == lastReadResponse_.end()){
			/**
			 * Server havn't send any response before.
			 * Just return the result.
			 */
			return ;
		}

		IPCWriteRequestMessage lastresponse = kv_pair->second;

		/**
		 * Check whether last response represent
		 * the operation has finished.
		 */
		bool isfinish = false;
		if(lastresponse.getStartingAddress() == 0
			&& lastresponse.getDataLength() == 0){
			isfinish = true;
		}

		bool keepsending = true;
		if(isfinish){
			// Remove the request from the queue.
			auto readrequests = readRequest_[path];
			readrequests.pop();

			if(readrequests.empty()){
				// No other more requests need to process.
				keepsending = false;
				readRequest_.erase(path);
				lastReadResponse_.erase(path)
			}
			else{
				// Get the next request.
				IPCReadRequestMessage nextrequest = readrequests.front();
				/**
			 	 * Get some info use for ceph.
			 	 */
			 	pro = nextrequest.getProperties();			 	
			}

		}
		else{
			/**
			 * TODO: Retrive the `workID` and `tranID`
			 */
		}

		if(!keepsending){
			return;
		}
	}

	if(objectSize == 0){
		// This is the final write.
		IPCWriteRequestMessage reply;
		reply.setPath(path);
		reply.setStartAddress(0);
		reply.setDataLength(0);
		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));
		// TODO: Update the last response.
		// TODO: Make sure that the client will comfirm this message, too.
		return;
	}

	size_t allocsize = objectSize;
	BFCAllocator::Offset memoryoffset;
	while((memoryoffset = readSMAllocator_.get()->Alloc(allocsize)) 
		== BFCAllocator::k_invalid_offset){
		allocsize = allocsize >> 1;
		/**
	 	 * TODO: handle situation that the share memory is total full.
	  	 */
		if(allocsize < 100){
			// TODO: we should not return here.
			return;
		}
	}
	Task task(username_,path,Task::READ,readSM_+memoryoffset,
				allocsize,tranID,workerID);
	/**
	 * TODO: If data check success, read data from ceph.
	 */

	// The following process may be done in the folly::future callback.

	/**
	 * Then reply write message to client.
	 */
	IPCWriteRequestMessage reply;
	reply.setPath(path);

	reply.setStartAddress(readSM_+memoryoffset);
	reply.setDataLength(allocsize);

	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf));
	// TODO: Update the last response
}

void ServerReadCallback::handleWriteRequest(
	std::unique_ptr<folly::IOBuf> data){
	IPCWriteRequestMessage write_msg;
	// If parse fail, stop processing.
	if(!write_msg.parse(std::move(data))){
		return;
	}

	/**
	 * TODO: check the bitmap.
	 */
	// Whether this write request is an new request.
	bool isnewcoming;
	
	if(isnewcoming){
		/**
	 	 * TODO: check user credentials and correctness of operation.
	 	 *
	 	 * Here we only grant each operation.
	 	 */

		if(!insertWriteRequest(write_msg)){
			// There is other write request processing
			// of the same object.
			return ;
		}
		else{
			/**
			 * TODO: get some info use for ceph.
			 */
		}
	}
	else{
		// It is a comfirm.
		auto kv_pair = lastWriteResponse_.find(write_msg.getPath());

		if(kv_pair == lastWriteResponse_.end()){
			/**
			 * Server havn't send any response before.
			 * Just return the result.
			 */
			return ;
		}

		IPCReadRequestMessage lastresponse = kv_pair->second;

		/**
		 * TODO: check whether this request represent
		 * 		 the operation has finished.
		 */
		bool isfinish;

		if(isfinish){
			// Remove the request from the queue.
			auto writerequests = writeRequest_[write_msg.getPath()];
			writerequests.pop();

			if(writerequests.empty()){
				// No other more requests need to process.
				writeRequest_.erase(write_msg.getPath());
			}
			else{
				// Get the next request.
				IPCWriteRequestMessage nextrequest = writerequests.front();
				/**
			 	 * TODO: get some info use for ceph.
			 	 */
			}

		}
		else{
			/**
			 * TODO: get some info use for ceph.
			 */
		}

	}

	/**
	 * TODO: If data check success, write data to ceph.
	 */


	// The following process may be done in the folly::future callback.

	/**
	 * Need to set last response.
	 */
}

void ServerReadCallback::handleCloseRequest(){
	/**
	 * Release share memory.
	 */
	munmap(readSM_,readSMSize_);
	munmap(writeSM_,writeSMSize_);
	shm_unlink(readSMName_);
	shm_unlink(writeSMName_);
	readSM_ = nullptr;
	writeSM_ = nullptr;

	/**
	 * Unregister the allocator.
	 */
	readSMAllocator_ = nullptr;
	writeSMAllocator_ = nullptr;

	username_.clear();
}

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn){
	auto res = readBuffer_.preallocate(minAllocBuf_, newAllocSize_);
    *bufReturn = res.first;
    *lenReturn = res.second;	
};

void ServerReadCallback::readDataAvailable(size_t len)noexcept{
	readBuffer_.postallocate(len);
	//std::cout << "read data available:"<< isBufferMovable() << std::endl;
	/**
	 * Get the message, now it still std::unique_ptr<folly::IOBuf>
	 */
	auto rec_iobuf = std::move(readBuffer_.pop_front());
	auto data = rec_iobuf->data();

	/**
	 * Check the message type
	 */
	IPCMessage::MessageType *type = (IPCMessage::MessageType *)data;
	switch(*type){
		case IPCMessage::MessageType::AUTH : 
			handleAuthticationRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::READ :
			handleReadRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::WRITE :
			handleWriteRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::CLOSE :
			handleCloseRequest();
			break;

		default:
			/**
			 * Except for the types above,
			 * Server should not receive any other type.
			 */
			return;
	}
	
};

}
