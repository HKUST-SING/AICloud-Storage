#include <iostream>
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
#include "../message/IPCDeleteRequestMessage.h"
#include "../message/IPCStatusMessage.h"
#include "../message/IPCCloseMessage.h"
#include "../utils/Hash.h"
#include "../utils/BFCAllocator.h"

namespace singaistorageipc{

void ServerReadCallback::sendStatus(
	uint32_t id,IPCStatusMessage::StatusType type){
	IPCStatusMessage reply;
	reply.setID(id);
	reply.setStatusType(type);
	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf));
}

void ServerReadCallback::handleAuthenticationRequest(
	std::unique_ptr<folly::IOBuf> data){

	IPCAuthenticationMessage auth_msg;
	// If parse fail, stop processing.
	if(!auth_msg.parse(std::move(data))){
		return;
	}

	if(readSM_ != nullptr || writeSM_ != nullptr){
		/**
		 * The user has authenticated before.
		 */

		// Or we can send a STATUS message here.
		sendStatus(auth_msg.getID(),IPCStatusMessage::StatusType::STAT_PROT);
		return ;
	}

	/**
	 * TODO: Send the user info to the authentication server.
	 *
	 * Here we only grant each authentication.
	 */
	std::string username = auth_msg.getUsername();
	char pw[32];
	auth_msg.getPassword(pw);

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
		delete(readSMName);
		readSMName = memName32(ctime(&t));
	}
	//memcpy(readSMName_,readSMName,32*sizeof(char));
	readSMName_ = readSMName;

	int writefd;
	while((writefd = shm_open(writeSMName,
		O_CREAT|O_EXCL|O_RDWR,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) == EEXIST){
		/*
		 * The object already exists. Change another name.
		 */
		t = time(NULL);
		delete(writeSMName);
		writeSMName = memName32(ctime(&t));
	}
	writeSMName_ = writeSMName;
	//memcpy(writeSMName_,writeSMName,32*sizeof(char));
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
	readSMAllocator_ = BFCAllocator::Create(0,readSMSize_);
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

	reply.setID(auth_msg.getID());

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
	bool isnewcoming = false;
	if(pro == 1){
		isnewcoming = true;
	}


	std::string path = read_msg.getPath();
	uint32_t workerID = 0;
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
			contextmap->second.pendingList_.emplace(read_msg);
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

			ReadRequestContext newcontext;
			newcontext.workerID_ = workerID;
			newcontext.remainSize_ = objectSize;
			readContextMap_.emplace(path,newcontext);
			contextmap = readContextMap_.find(path);
		}
	}
	else{ // if(isnewcoming)
		// It is a comfirm.
		if(contextmap == readContextMap_.end()){
			/**
			 * There is no context. It must be an error.
			 */
			sendStatus(read_msg.getID(),
				IPCStatusMessage::StatusType::STAT_AMBG);
			return;
		}

		IPCWriteRequestMessage lastresponse 
			= contextmap->second.lastResponse_;

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
			auto readrequests = contextmap->second.pendingList_;

			if(readrequests.empty()){
				// No other more requests need to process.
				keepsending = false;
				readContextMap_.erase(path);
			}
			else{
				// Get the next request.
				IPCReadRequestMessage nextrequest = readrequests.front();
				contextmap->second.pendingList_.pop();

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
			 	contextmap->second.workerID_ = workerID;
			 	contextmap->second.remainSize_ = objectSize;

			}

		}
		else{ //if(isfinish)
			/**
			 * Retrive the `workerID`,`tranID` and `objectSize`
			 */
			workerID = contextmap->second.workerID_;
			objectSize = contextmap->second.remainSize_;

			/**
			 * Release memory.
			 */
			readSMAllocator_->Deallocate(
				lastresponse.getStartingAddress()-(uint64_t)readSM_);
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
		contextmap->second.lastResponse_ = reply;
		return;
	}

	size_t allocsize = objectSize;
	BFCAllocator::Offset memoryoffset;
	while((memoryoffset = readSMAllocator_->Allocate(allocsize)) 
		== BFCAllocator::k_invalid_offset){
		allocsize = allocsize >> 1;
		/**
	 	 * TODO: handle situation that the share memory is total full.
	  	 */
		if(allocsize < 10){
			// TODO: we should not return here.
			return;
		}
	}
	Task task(username_,path,Task::OpType::READ,
		(uint64_t)readSM_+memoryoffset,
		allocsize,read_msg.getID(),workerID);
	/**
	 * TODO: If data check success, read data from ceph.
	 */

	// The following process may be done in the folly::future callback.

	/**
	 * Then reply write message to client.
	 */
	IPCWriteRequestMessage reply;
	reply.setPath(path);

	reply.setStartAddress((uint64_t)readSM_+memoryoffset);
	reply.setDataLength(allocsize);

	reply.setID(read_msg.getID());

	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf));
	// Update the context
	/**
	 * TODO: the `remainSize` should retrive from ceph.
	 */
	contextmap->second.remainSize_ -= allocsize;
	contextmap->second.lastResponse_ = reply;
	/**
	 * TODO: this `workerID` should retrive from ceph.
	 */
	contextmap->second.workerID_ = workerID;
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

	IPCReadRequestMessage reply;	
	std::string path = write_msg.getPath();
	reply.setPath(path);
	reply.setID(write_msg.getID());
	reply.setProperties(0);

	if(pro == 1){
		/**
		 * TODO: The flag set. It means this is the first 
		 * 		 message of a new request. We need authenticate 
		 * 		 at the authentication server.
		 * 		 The total size of written object is set in 
		 *		 `dataLength_`, while the `startAddress_` is 0.
		 *		 Here, we just grant every request.
		 */
		reply.setProperties(1);

		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));
		return;
	}

	uint32_t workerID = 0;

	auto contextmap = writeContextMap_.find(path);

	bool isfinish = false;
	if(write_msg.getStartingAddress() == 0
		&& write_msg.getDataLength() == 0){
		isfinish = true;
	} 

	WriteRequestContext writecontext;

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
			/*
			if(!contextmap->second->pendingList_.empty()){
				 // There are still other request.
				writecontext = contextmap->second;
				write_msg = writecontext->pendingList_.front();
				writecontext->pendingList_.pop();
			}*/
				/**
				 * There are no other request.
				 */
			writecontext = contextmap->second;
			workerID = writecontext.workerID_;
			writeContextMap_.erase(path);
		}
		else{
			sendStatus(write_msg.getID(),
				IPCStatusMessage::StatusType::STAT_AMBG);
		}
		return;
	}
	else{ //if(isfinish)
		if(contextmap == writeContextMap_.end()){
			/**
			 * This is the first write
			 */
			writeContextMap_.emplace(path,writecontext);
			contextmap = writeContextMap_.find(path);
		}
		else{
			/**
			 * Retrive `tranID` and `workerID`.
			 */
			writecontext = contextmap->second;
			workerID = writecontext.workerID_;
		}
	}

	/**
	 * TODO: If data check success, write data to ceph.
	 */
	Task task(username_,path,Task::OpType::WRITE,
		write_msg.getStartingAddress(),
		write_msg.getDataLength(),write_msg.getID(),workerID);

	// The following process may be done in the folly::future callback.

	auto send_iobuf2 = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf2));

	/**
	 * Update context.
	 */
	contextmap->second.workerID_ = workerID;
	contextmap->second.lastResponse_ = reply;
}

void ServerReadCallback::handleDeleteRequest(
	std::unique_ptr<folly::IOBuf> data){
	IPCDeleteRequestMessage delete_msg;
	// If parse fail, stop processing.
	if(!delete_msg.parse(std::move(data))){
		return;
	}

	std::string path = delete_msg.getPath();
	/**
	 * TODO: check whether this operation is vaild.
	 *
	 * Now, we just grant each operation.
	 */

	/**
	 * TODO: send the delete operation to the ceph
	 *       and save the `tranID`.
	 */
	Task task(username_,path,Task::OpType::DELETE,0,0,delete_msg.getID());

	/**
	 * Reply the result.
	 */
	sendStatus(delete_msg.getID(),
		IPCStatusMessage::StatusType::STAT_SUCCESS);
}

void ServerReadCallback::handleCloseRequest(
	std::unique_ptr<folly::IOBuf> data){
	IPCCloseMessage close_msg;
	// If parse fail, stop processing.
	if(!close_msg.parse(std::move(data))){
		return;
	}
	sendStatus(close_msg.getID(),
		IPCStatusMessage::StatusType::STAT_SUCCESS);
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
			handleAuthenticationRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::READ :
			handleReadRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::WRITE :
			handleWriteRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::DELETE :
			handleDeleteRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::CLOSE :
			handleCloseRequest(std::move(rec_iobuf));
			break;

		default:
			/**
			 * Except for the types above,
			 * Server should not receive any other type.
			 */
			return;
	}
	
};

void ServerReadCallback::readEOF() noexcept{
	readBuffer_.clear();
    readContextMap_.clear();
    writeContextMap_.clear();

    /**
	 * TODO: wait uutil all the pengding operation finish.
	 */
	/**
	 * Release share memory.
	 */
	if(readSM_ != nullptr){
		munmap(readSM_,readSMSize_);	
		shm_unlink(readSMName_);
		readSM_ = nullptr;
		delete(readSMName_);
		readSMName_ = nullptr;
	}
	
	if(writeSM_ != nullptr){
		munmap(writeSM_,writeSMSize_);
		shm_unlink(writeSMName_);
		writeSM_ = nullptr;
		delete(writeSMName_);
		writeSMName_ = nullptr;
	}

	/**
	 * Unregister the allocator.
	 */
	readSMAllocator_ = nullptr;
	//writeSMAllocator_ = nullptr;

	username_.clear();
}

}
