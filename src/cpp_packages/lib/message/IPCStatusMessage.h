/**
 * This message is used for server responding error in 
 * authentication or operation.
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCStatusMessage : public IPCMessage{
public:

	enum class StatusType : uint16_t{
		STAT_SUCCESS     = 0,   // notifies success
		STAT_CLOSE       = 1,   // require to close IPC and release resources
		STAT_AUTH_USER   = 2,   // authentication problem (username)
		STAT_AUTH_PASS   = 3,   // authentication problem (password)
		STAT_PATH	     = 3,   // data path problem ('no such path')
		STAT_DENY	     = 4,   // denied access to the data at 'path'
		STAT_QUOTA       = 5,   // user has exceeded his/her data quota
		STAT_PROT        = 6,   // the used protocol is not supported
		STAT_AMBG        = 254, // cannot understand previously sent request
		STAT_INTER       = 255 // internal system error (release resources)
	};

	IPCStatusMessage(){
		msgType_ = IPCMessage::MessageType::STATUS;
	};

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::STATUS){
			return false;
		}

		data = parseHead(data);

		if(length != msgLength_){
			return false;
		}

		memcpy(&statusType_,data,sizeof(StatusType));

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		void *buffer = (void*)malloc(computeLength());
		void *tmp = buffer;

		tmp = createMsgHead(tmp);

		memcpy(tmp,&statusType_,sizeof(StatusType));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};

	void setStatusType(StatusType type){
		statusType_ = type;
	};

	StatusType getStatusType(){
		return statusType_;
	};

private:
	StatusType statusType_;

	uint32_t computeLength() override{
		msgLength_ = computeHeadLength()
					+ sizeof(StatusType);

		return msgLength_;
	};
};

}
