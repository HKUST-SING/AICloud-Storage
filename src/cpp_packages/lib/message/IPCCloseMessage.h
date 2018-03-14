/*
 * Define the message for closing connection.
 */

#pragma once

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCCloseMessage : public IPCMessage{
public:
	IPCMessage(){
		msgType_ = IPCMessage::MessageType::CLOSE;
	}

	bool parse(std::unique_ptr<folly::IOBuf>) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::CLOSE){
			return false;
		}

		data += sizeof(IPCMessage::MessageType);
		memcpy(&msgLength_,data,sizeof(uint32_t));

		if(length != msgLength_){
			return false;
		}

		return true;
	}

	std::unique_ptr<folly::IOBuf> createMsg() override{
		void *buffer = (void*)malloc(computeLength());
		void *tmp = buffer;

		memcpy(tmp,&msgType_,sizeof(IPCMessage::MessageType));
		tmp += sizeof(IPCMessage::MessageType);

		memcpy(tmp,&msgLength_,sizeof(uint32_t));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	}
private:
	uint32_t computeLength() override{
		msgLength_ = sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t);

		return msgLength_;
	}

};

}