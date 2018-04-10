#include <ctime>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <sys/stat.h>
#include <sys/mman.h>

#include <folly/io/IOBufQueue.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/futures/Future.h>

#include "ServerReadCallback.h"
#include "ServerWriteCallback.h"
#include "include/Task.h"
#include "include/CommonCode.h"
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
#include "remote/Security.h"
#include "remote/Authentication.h"

namespace singaistorageipc{

void ServerReadCallback::sendStatus(
	uint32_t id,CommonCode::IOStatus type)
{
	IPCStatusMessage reply;
	reply.setID(id);
	reply.setStatusType(type);
	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb_,std::move(send_iobuf));
	DLOG(INFO) << "send a status message";
}

//======================== Authentication ======================

void ServerReadCallback::callbackAuthenticationRequest(Task task)
{
	/**
	 * Erase future from the pool.
	 */
	futurePool_.erase(task.tranID_);

	/**
	 * Check result and send back to the client.
	 *
	 * Now we just send success.
	 */

	if(task.opStat_ == CommonCode::IOStatus::STAT_SUCCESS){
		DLOG(INFO) << "permit authentication";
		/**
		 * Set the `username_`
		 */
		username_ = task.username_;

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
		DLOG(INFO) << "initialize share memory successfully";
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

		// get transaction id from task.
		reply.setID(task.tranID_);

		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));
	} // if(issuccess)
	else{
		DLOG(INFO) << "deny authentication";
		sendStatus(task.tranID_,task.opStat_);
	}
}

void ServerReadCallback::handleAuthenticationRequest(
	std::unique_ptr<folly::IOBuf> data)
{

	IPCAuthenticationMessage auth_msg;

	// If parse fail, stop processing.
	if(!auth_msg.parse(std::move(data))){
		LOG(WARNING) << "fail to parse authentication request";
		return;
	}

	uint32_t tranID = auth_msg.getID();

	if(readSM_ != nullptr || writeSM_ != nullptr){
		/**
		 * The user has authenticated before.
		 */
		LOG(WARNING) << "user has double authentication";
		// Or we can send a STATUS message here.
		sendStatus(tranID,CommonCode::IOStatus::ERR_PROT);
		return ;
	}

	/**
	 * Send the user info to the authentication server.
	 *
	 * Here we only grant each authentication.
	 */
	std::string username = auth_msg.getUsername();
	char pw[32];
	auth_msg.getPassword(pw);

	std::string password(pw,32);
	password_ = password;

	UserAuth auth(username,password);
	folly::Future<Task> future = sec_->clientConnect(auth);
	future.via(folly::EventBaseManager::get()->getEventBase())
		  .then([=](Task t){
		  		t.tranID_ = tranID;
		  		this->callbackAuthenticationRequest(t);
		  });
	
	futurePool_[tranID] = future;
};

//======================== Read ============================

// Declaration
void doReadCredential(uint32_t tranID,const std::string &path);

bool allocatSM(size_t &allocsize, BFCAllocator::Offset &memoryoffset)
{
	while((memoryoffset = readSMAllocator_->Allocate(allocsize)) 
		== BFCAllocator::k_invalid_offset)
	{
		allocsize = allocsize >> 1;
		if(allocsize < 10){
			// the share memory is totally full.
			return false;
		}
	}	
	return true;
}

bool finishThisReadRequest(const std::string& path)
{
	auto contextmap = readContextMap_.find(path);
	auto readrequests = contextmap->second.pendingList_;

	if(readrequests.empty()){
		// No other more requests need to process.
		readContextMap_.erase(path);
		return true;
	}
	else{
		// Get the next request.
		IPCReadRequestMessage nextrequest = readrequests.front();
		contextmap->second.pendingList_.pop();

		/**
	 	 * Get some info use for ceph.
	 	 */
	 	uint32_t pro = nextrequest.getProperties();	
	 	uint32_t tranID = nextrequest.getID();

		/**
	 	 	 * Check user credentials and correctness of operation.
	 	 	 * 		 update `objectSize`
	 	 	 *
	 	 	 * Here we only grant each operation.
	 	 	 */
	 	return doReadCredential(tranID,path);
	}
}

void ServerReadCallback::callbackReadRequest(Task task)
{
	/**
	 * Erase future from the pool.
	 */
	futurePool_.erase(task.tranID_);
	// Update the context
	auto contextmap = readContextMap_.find(task.path_);

	if(task.opStat_ == CommonCode::IOStatus::STAT_SUCCESS){
		DLOG(INFO) << "permit READ request";
		/**
		 * Then reply write message to client.
		 */
		IPCWriteRequestMessage reply;
		reply.setPath(task.path_);

		reply.setStartAddress(task.dataAddr_);
		reply.setDataLength(task.dataSize_);

		reply.setID(task.tranID_);

		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));

		/**
		 * the `remainSize` should retrive from ceph.
		 */
		contextmap->second.remainSize_ = task.objSize_;
		contextmap->second.lastResponse_ = reply;
		/**
		 * the `workerID` should retrive from ceph.
		 */
		contextmap->second.workerID_ = task.workerID_;
	}
	else{
		DLOG(INFO) << "deny READ request";
		sendStatus(task.tranID_,task.opStat_);

		/**
		 * Release memory.
		 */
		readSMAllocator_->Deallocate(task.dataAddr_-(uint64_t)readSM_);

		/**
		 * Keep sending read request to worker.
		 *
		 * If return true, meaning there may have extra memory.
		 * Try to send unallocated request.
		 */
		if(finishThisReadRequest(task.path_)){
			for(auto iterator = unallocatedRequest_.begin();
				iterator != unallocatedRequest_.end();){
				if(!passReadRequesttoTask(iterator->first,iterator->second)){
					break;
				}
				unallocatedRequest_.erase(iterator++);
			}
		}
	}
}

bool passReadRequesttoTask(
	uint32_t tranID,
	const std::unordered_map<std::string,
					ReadRequestContext>::iterator contextmap)
{
	uint64_t objectSize = contextmap->second.remainSize_;
	std::string path = contextmap->first;
	if(objectSize == 0){
		// This is the final write.
		IPCWriteRequestMessage reply;
		reply.setPath(path);
		reply.setID(tranID);
		reply.setStartAddress(0);
		reply.setDataLength(0);
		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));
		// Update the last response.
		contextmap->second.lastResponse_ = reply;
		return true;
	}

	size_t allocsize
	if(objectSize >= std::numeric_limits<uint32_t>::max()){
		allocsize = defaultAllocSize_;
	} 
	else{
		allocsize = objectSize;
	}

	BFCAllocator::Offset memoryoffset;

	if(!allocatSM(allocsize,memoryoffset))
	{
		/**
		 * The share memory is totally full.
		 * Store the request and allocat it 
		 * again when there is memory release.
		 */
		unallocatedRequest_[tranID] = contextmap;
		return false;
	}

	Task task(username_,path,CommonCode::IOOpCode::OP_READ,
		(uint64_t)readSM_+memoryoffset,
		allocsize,tranID,contextmap->second.workerID_);
	
	/**
	 * TODO: call worker to do the task
	 */
	folly::Future<Task> future = worker_->sendTask(task);

 	future.via(folly::EventBaseManager::get()->getEventBase())
 		  .then(&ServerReadCallback::callbackReadRequest,this);
 	futurePool_[tranID] = future;

	return true;
}

bool doReadCredential(uint32_t tranID,const std::string &path)
{
	auto contextmap = readContextMap_.find(path);
	if(contextmap == readContextMap_.end()){
		ReadRequestContext newcontext;
		readContextMap_.emplace(path,newcontext);
		contextmap = readContextMap_.find(path);
	}
	contextmap->second.workerID_ = 0;
	contextmap->second.remainSize_ = std::numeric_limits<uint64_t>::max();
	return passReadRequesttoTask(tranID,contextmap);
}

void ServerReadCallback::handleReadRequest(
	std::unique_ptr<folly::IOBuf> data)
{
	IPCReadRequestMessage read_msg;
	// If parse fail, stop processing.
	if(!read_msg.parse(std::move(data))){
		LOG(WARNING) << "fail to parse read request";
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


	uint32_t tranID = read_msg.getID();
	std::string path = read_msg.getPath();
	uint32_t workerID = 0;

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
	 	 	 * 
	 	 	 * Check user credentials and correctness of operation.
	 	 	 * 		 update `objectSize`
	 	 	 *
	 	 	 * Here we only grant each operation.
	 	 	 */
			doReadCredential(tranID,path);
		}
	}
	else{ // if(isnewcoming)
		// It is a comfirm.
		if(contextmap == readContextMap_.end()){
			/**
			 * There is no context. It must be an error.
			 */
			sendStatus(read_msg.getID(),
				CommonCode::IOStatus::ERR_PROT);
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

		if(isfinish){
			finishThisReadRequest(path);
		}
		else{ //if(isfinish)
			/**
			 * Release memory.
			 */
			readSMAllocator_->Deallocate(
				lastresponse.getStartingAddress()-(uint64_t)readSM_);

			/**
			 * Keep sending read request to worker.
			 *
			 * If return true, meaning there may have extra memory.
			 * Try to send unallocated request.
			 */
			if(passReadRequesttoTask(tranID,contextmap)){
				for(auto iterator = unallocatedRequest_.begin();
					iterator != unallocatedRequest_.end();){
					if(!passReadRequesttoTask(iterator->first,iterator->second)){
						break;
					}
					unallocatedRequest_.erase(iterator++);
				}
			}
		}
	}	
}

//====================== Write =============================

void doWriteRequestCallback(Task task,uint32_t pro)
{
	futurePool_.erase(task.tranID_);

	if(task.opStat_ == CommonCode::IOStatus::STAT_SUCCESS){
		DLOG(INFO) << "permit WRITE request";
		IPCReadRequestMessage reply;
		reply.setPath(task.path_);
		reply.setID(task.tranID_);
		reply.setProperties(pro);

		auto send_iobuf = reply.createMsg();
		socket_->writeChain(&wcb_,std::move(send_iobuf));

		/**
		 * Update context.
		 */
		auto contextmap = writeContextMap_.find(task.path_);
		contextmap->second.workerID_ = task.workerID_;
		contextmap->second.lastResponse_ = reply;


		if(pro != 1 && task.dataAddr_ == 0 && task.dataSize_ == 0){
			writeContextMap_.erase(task.path_);
		}
	}
	else{
		DLOG(INFO) << "deny WRITE request";
		writeContextMap_.erase(task.path_);
		sendStatus(task.tranID_,task.opStat_);		
	}
}

void ServerReadCallback::callbackWriteRequest(Task task)
{
	doWriteRequestCallback(task,0);
}

void ServerReadCallback::callbackWriteCredential(Task task)
{
	doWriteRequestCallback(task,1);
}

void ServerReadCallback::handleWriteRequest(
	std::unique_ptr<folly::IOBuf> data)
{
	IPCWriteRequestMessage write_msg;
	// If parse fail, stop processing.
	if(!write_msg.parse(std::move(data))){
		LOG(WARNING) << "fail to parse WRITE request";
		return;
	}

	/**
	 * TODO: check the bitmap.
	 */
	uint32_t pro = write_msg.getProperties();
	
	std::string path = write_msg.getPath();
	uint32_t tranID = write_msg.getID();

	auto contextmap = writeContextMap_.find(path);

	if(contextmap == writeContextMap_.end()){
		/**
		 * This is the first write
		 */
		WriteRequestContext writecontext;
		writecontext.workerID_ = 0;
		writeContextMap_.emplace(path,writecontext);
		contextmap = writeContextMap_.find(path);
	}

	/**
	 * Write data to ceph.
	 */
	Task task(username_,path,CommonCode::IOOpCode::OP_WRITE,
		write_msg.getStartingAddress(),
		write_msg.getDataLength(),tranID,contextmap->second.workerID_);
	folly::Future<Task> future = worker_->sendTask(task);
	futurePool_[tranID] = future;
	
	if(pro == 1){
		/**
		 *	The flag set. It means this is the first 
		 *	message of a new request. We need authenticate 
		 *	at the authentication server.
		 *	The total size of written object is set in 
		 *	`dataLength_`, while the `startAddress_` is 0.
		 *	Here, we just grant every request.
		 */
 	 	future.via(folly::EventBaseManager::get()->getEventBase())
 	 		  .then(&ServerReadCallback::callbackWriteCredential,this);
	}
	else{
 		future.via(folly::EventBaseManager::get()->getEventBase())
 		  	  .then(&ServerReadCallback::callbackWriteRequest,this);
 	}
}


//================== Delete ===================================

void ServerReadCallback::callbackDeleteRequest(Task task)
{
	futurePool_.erase(task.tranID_);

	/**
	 * Reply the result.
	 */
	sendStatus(task.tranID_,task.opStat_);
}

void ServerReadCallback::handleDeleteRequest(
	std::unique_ptr<folly::IOBuf> data)
{
	IPCDeleteRequestMessage delete_msg;
	// If parse fail, stop processing.
	if(!delete_msg.parse(std::move(data))){
		LOG(WARNING) << "fail to parse DELETE request";
		return;
	}

	std::string path = delete_msg.getPath();
	uint32_t tranID = delete_msg.getID();

	/**
	 * Send the request to the worker.
	 */
	Task task(username_,path
			 ,CommonCode::IOOpCode::OP_DELETE
			 ,0,0,tranID);

	folly::Future<Task> future = worker_->sendTask(task);
	future.via(folly::EventBaseManager::get()->getEventBase())
 		  .then(&ServerReadCallback::callbackDeleteRequest,this);
 	futurePool_[task.tranID_] = future;
}

//===================== Close =======================

void ServerReadCallback::handleCloseRequest(
	std::unique_ptr<folly::IOBuf> data)
{
	IPCCloseMessage close_msg;
	// If parse fail, stop processing.
	if(!close_msg.parse(std::move(data))){
		LOG(WARNING) << "fail to parse CLOSE request";
		return;
	}
	sendStatus(close_msg.getID(),
		CommonCode::IOStatus::STAT_SUCCESS);
}

//================================================================

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn)
{
	auto res = readBuffer_.preallocate(minAllocBuf_, newAllocSize_);
    *bufReturn = res.first;
    *lenReturn = res.second;	
}

void ServerReadCallback::readDataAvailable(size_t len)noexcept
{
	readBuffer_.postallocate(len);
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
			DLOG(INFO) << "process AUTH request";
			handleAuthenticationRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::READ :
			DLOG(INFO) << "process READ request";
			handleReadRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::WRITE :
			DLOG(INFO) << "process WRITE request"
			handleWriteRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::DELETE :
			DLOG(INFO) << "process DELETE request"
			handleDeleteRequest(std::move(rec_iobuf));
			break;

		case IPCMessage::MessageType::CLOSE :
			DLOG(INFO) << "process CLOSE request";
			handleCloseRequest(std::move(rec_iobuf));
			break;

		default:
			/**
			 * Except for the types above,
			 * Server should not receive any other type.
			 */
			DLOG(WARNING) << "receive unknown request";
			return;
	}	
}

void ServerReadCallback::readEOF() noexcept
{
	DLOG(INFO) << "start to close ServerReadCallback";
	readBuffer_.clear();
    readContextMap_.clear();
    writeContextMap_.clear();

    /**
	 * TODO: wait until all the pengding operation finish.
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

	username_.clear();
	password_.clear();
	sec_ = nullptr;

	/**
	 * Cancel unfinished Future.
	 */
	for(auto kv : futurePool_){
		kv.second.cancel();
	}
	futurePool_.clear();

	unallocatedRequest_.clear();

	DLOG(INFO) << "closed ServerReadCallback";
}

}