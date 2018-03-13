#pragma once
\
#include <folly/io/IOBufQueue.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>

#include "ServerReadCallback.h"
#include "ServerWriteCallback.h"
#include "../message/IPCMessage.h"
#include "../message/IPCAuthenticationMessage.h"
#include "../message/IPCConnectionReplyMessage.h"
#include "../message/IPCReadRequestMessage.h"
#include "../message/IPCWriteRequestMessage.h"
#include "../message/IPCStatusMessage.h"

namespace singaistorageipc{

bool ServerReadCallback::insertReadRequest(
	IPCReadRequestMessage msg){
	auto search = readRequest_->find(msg.getPath());
	if(search == readRequest_->end()){
		std::queue<IPCReadRequestMessage> queue;
		queue.emplace(msg);
		readRequest_->emplace(msg.getPath(),queue);
		return true;
	}
	else{
		search->second.emplace(msg);
		return false;
	}
}

bool ServerReadCallback::insertWriteRequest(
	IPCWriteRequestMessage msg){
	auto search = writeRequest_->find(msg.getPath());
	if(search == writeRequest_->end()){
		std::queue<IPCWriteRequestMessage> queue;
		queue.emplace(msg);
		writeRequest_->emplace(msg.getPath(),queue);
		return true;
	}
	else{
		search->second.emplace(msg);
		return false;
	}
}

void ServerReadCallback::handleAuthticationRequest(
	folly::unique_ptr<IOBuf> data){
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

	/**
	 * TODO: Check result and send back to the client.
	 *
	 * Now we just send success.
	 */
	IPCConnectionReplyMessage reply;
	/**
	 * TODO: Fulfill the content.
	 */
	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb,std::move(send_iobuf));
};

void ServerReadCallback::handleReadRequest(
	folly::unique_ptr<IOBuf> data){
	IPCReadRequestMessage read_msg;
	// If parse fail, stop processing.
	if(!read_msg.parse(std::move(data))){
		return;
	}

	/**
	 * TODO: check the bitmap.
	 */
	// Whether this read request is an new request.
	bool isnewcoming;

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
		else{
			/**
			 * TODO: get some info use for ceph.
			 */
		}
	}
	else{
		// It is a comfirm.
		auto kv_pair = lastReadResponse_.find(read_msg.getPath());

		if(kv_pair == lastReadResponse_.end()){
			/**
			 * Server havn't send any response before.
			 * Just return the result.
			 */
			return ;
		}

		IPCWriteRequestMessage lastresponse = kv_pair->second.

		/**
		 * TODO: check whether last response represent
		 * 		 the operation has finished.
		 */
		bool isfinish;
		bool keepsending = true;
		if(isfinish){
			// Remove the request from the queue.
			auto readrequests = readRequest_[read_msg.getPath()];
			readrequests.pop();

			if(readrequests.empty()){
				// No other more requests need to process.
				keepsending = false;
				readRequest_.erase(read_msg.getPath());
			}
			else{
				// Get the next request.
				IPCReadRequestMessage nextrequest = readrequests.front();
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


		if(!keepsending){
			return;
		}
	}

	/**
	 * TODO: If data check success, read data from ceph.
	 */

	// The following process may be done in the folly::future callback.

	/**
	 * Then reply write message to client.
	 */
	IPCWriteRequestMessage reply;
	reply.setPath(read_msg.getPath());
	/**
	 * TODO: Need more setting and set last response.
	 */

	auto send_iobuf = reply.createMsg();
	socket_->writeChain(&wcb,std::move(send_iobuf));
}

void ServerReadCallback::handleWriteRequest(
	folly::unique_ptr<IOBuf> data){
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

		IPCWriteRequestMessage lastresponse = kv_pair->second.

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
				writeRequest_.erase(read_msg.getPath());
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

void ServerReadCallback::getReadBuffer(void** bufReturn, size_t* lenReturn){
	auto res = readBuffer_.preallocate(100, 500);
    *bufReturn = res.first;
    *lenReturn = res.second;	
};

void ServerReadCallback::readDataAvailable(size_t len)noexcept{
	readBuffer_.postallocate(len);
	std::cout << "read data available:"<< isBufferMovable() << std::endl;
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

		default:
			/**
			 * Except for the types above,
			 * Server should not receive any other type.
			 */
			return;
	}
	
};

}