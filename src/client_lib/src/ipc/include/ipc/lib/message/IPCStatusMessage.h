/**
 * This message is used for server responding error in 
 * authentication or operation.
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"
#include "include/CommonCode.h"

namespace singaistorageipc{

class IPCStatusMessage : public IPCMessage{
public:

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

		memcpy(&statusType_,data,sizeof(CommonCode::IOStatus));

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		char *buffer = (char*)malloc(computeLength());
		char *tmp = buffer;

		tmp = createMsgHead(tmp);

		memcpy(tmp,&statusType_,sizeof(CommonCode::IOStatus));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};

	void setStatusType(CommonCode::IOStatus type){
		statusType_ = type;
	};

	CommonCode::IOStatus getStatusType(){
		return statusType_;
	};

private:
	CommonCode::IOStatus statusType_;

	uint32_t computeLength() override{
		msgLength_ = computeHeadLength()
					+ sizeof(CommonCode::IOStatus);

		return msgLength_;
	};
};

}
