
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
#include <memory>
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
	///writeSMAllocator_ = BFCAllocator::create(1,writeSMSize_);

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
	uint32_t workerID = 0;
	uint32_t tranID;
	uint32_t objectSize;

	auto contextmap = readContextMap_.find(path);
	if(isnewcoming){
	 	/**
	 	 * Find if there are other read request processing
	 	 * on the same object.
	 	 */ 
		if(contextmap != readContextMap_.end()){
			/**
			 * There is other read request processing
			 * on the same object.
			 */
			contextmap->second->pendingList.emplace(read_msg);
			return ;
		}
		else{
			/**
			 * It is the first request.
			 */
			/**
	 	 	 * TODO: check user credentials and correctness of operation.
	 	 	 * 		 update `objectSize`
	 	 	 *
	 	 	 * Here we only grant each operation.
	 	 	 */
	 		objectSize = std::rand();

			tranID = std::rand();
			auto newcontext = std::make_shared<ReadRequestContext>();
			newcontext->workerID_ = workerID;
			newcontext->tranID_ = tranID;
			newcontext->remainSize_ = objectSize;
			readContextMap_.emplace(path,newcontext);
		}
	}
	else{ // if(isnewcoming)
		// It is a comfirm.
		if(contextmap == readContextMap_.end()){
			/**
			 * There is no context. It must be an error.
			 */
			return;
		}

		IPCWriteRequestMessage lastresponse 
			= contextmap->second->lastresponse_;

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
			// check the request queue size.
			auto readrequests = contextmap->second->pendingList_;

			if(readrequests.empty()){
				// No other more requests need to process.
				keepsending = false;
				readContextMap_.erase(path);
			}
			else{
				// Get the next request.
				IPCReadRequestMessage nextrequest = readrequests.front();
				contextmap->second->pendingList_.pop();

				/**
	 	 	 	 * TODO: check user credentials and correctness of operation.
	 	 	 	 * 		 update `objectSize`
	 	 	 	 *
	 	 	 	 * Here we only grant each operation.
	 	 	 	 */
	 			objectSize = std::rand();

				/**
			 	 * Get some info use for ceph.
			 	 */
			 	pro = nextrequest.getProperties();	
			 	/**
			 	 * Update context.
			 	 */		 	
			 	tranID = std::rand();
			 	contextmap->second->workerID_ = workerID;
			 	contextmap->second->tranID_ = tranID;
			 	contextmap->second->remainSize_ = objectSize;

			}

		}
		else{ //if(isfinish)
			/**
			 * Retrive the `workerID`,`tranID` and `objectSize`
			 */
			workerID = contextmap->second->workerID_;
			tranID = contextmap->second->tranID_;
			objectSize = contextmap->second->remainSize_;
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
		// Update the last response.
		contextmap->second->lastresponse_ = reply;
		// TODO: Make sure that the client will comfirm this message, too.
		return;
	}

	size_t allocsize = objectSize;
	BFCAllocator::Offset memoryoffset;
	while((memoryoffset = readSMAllocator_->Alloc(allocsize)) 
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
	// Update the context
	contextmap->second->remainSize_ -= allocsize;
	contextmap->second->lastresponse_ = reply;
	/**
	 * TODO: this `workerID` should retrive from ceph.
	 */
	contextmap->second->workerID_ = workerID;
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
	uint32_t pro = write_msg.getProperties();

	std::string path = write_msg.getPath();
	uint32_t workerID = 0;
	uint32_t tranID;

	auto contextmap = writeContextMap_.find(path);

	bool isfinish = false;
	if(write_msg.getStartingAddress() == 0
		&& write_msg.getDataLength() == 0){
		isfinish = true;
	} 

	std::shared_ptr<writeRequestContext> writecontext;
	bool isfirstwrite = false;
	IPCReadRequestMessage reply;
	reply.setPath(path);

	/**
	 * TODO: need discuss.
	 */
	if(isfinish){
		/**
		 * TODO: Set properties with flag is unset.
		 */
		/**
		 * Reply to the client.
		 */
		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));

		if(contextmap != writeContextMap_.end()){
			/**
			 * Check whether there is pending request.
			 */
			if(!contextmap->second.pendingList_.empty()){
				/**
				 * There are still other request.
				 */
				writecontext = contextmap->second;
				write_msg = writecontext.pendingList_.front();
				writecontext.pendingList_.pop();
				//Goto the place of new arriving.
				goto NEWHANDLEPOINT;
			}
			else{
				/**
				 * There are no other request.
				 */
				writeContextMap_.erase(path);
				return;
			}
		}
	}
	else{ //if(isfinish)
		if(contextmap == writeContextMap_.end()){
			/**
			 * This is the first write
			 */
			writecontext = std::make_shared<ReadRequestContext>();
			writeContextMap_.emplace(path,writecontext);
NEWHANDLEPOINT:
			tranID = std::rand();
			isfirstwrite = true;
		}
		else{
			/**
			 * Retrive `tranID` and `workerID`.
			 */
			writecontext = contextmap->second;
			tranID = writecontext.tranID_;
			workerID = writecontext.workerID_;
		}
	}

	/**
	 * TODO: If data check success, write data to ceph.
	 */
	Task task(username_,path,Task::WRITE,write_msg.getStartingAddress(),
				write_msg.getDataLength(),tranID,workerID);

	// The following process may be done in the folly::future callback.
	if(isfirstwrite){
		/**
		 * TODO: set the flag.
		 */
	}
	auto send_iobuf2 = reply.createMsg();
	socket_.writeChain(&wcb_,std::move(send_iobuf2));

	/**
	 * Update context.
	 */
	writecontext.tranID_ = tranID;
	writecontext.workerID_ = workerID;
	writecontext.lastresponse_ = reply;
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
	//writeSMAllocator_ = nullptr;

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
