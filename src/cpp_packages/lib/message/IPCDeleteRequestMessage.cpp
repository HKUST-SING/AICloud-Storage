/*
 * Define the message for delete object.
 */

#pragma once

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCDeleteRequestMessage : public IPCMessage{

public:
	IPCReadRequestMessage(){
		msgType_ = IPCMessage::MessageType::DELETE;
		pathLength_ = 0;
	};

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::DELETE){
			return false;
		}

		data += sizeof(IPCMessage::MessageType);
		memcpy(&msgLength_,data,sizeof(uint32_t));

		if(length != msgLength_){
			return false;
		}

		data += sizeof(uint32_t);
		memcpy(&pathLength_,data,sizeof(uint16_t));

		data += sizeof(uint16_t);
		pathVal_ = std::string((char*)data,pathLength_);

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		void *buffer = (void*)malloc(computeLength());
		void *tmp = buffer;

		memcpy(tmp,&msgType_,sizeof(IPCMessage::MessageType));
		tmp += sizeof(IPCMessage::MessageType);

		memcpy(tmp,&msgLength_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,&pathLength_,sizeof(uint16_t));
		tmp += sizeof(uint16_t);

		memcpy(tmp,pathVal_.c_str(),pathLength_ * sizeof(char));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};

	void setPath(const std::string& val){
		pathVal_ = val;
		pathLength_ = pathVal_.length() * sizeof(char);
	};

	uint16_t getPathLength(){
		return pathLength_;
	};

	std::string getPath(){
		return pathVal_;
	};

private:
	uint16_t pathLength_;
	std::string pathVal_;

	uint32_t computeLength() override{
		msgLength_ = sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t)
					+ sizeof(uint16_t)
					+ (pathLength_ * sizeof(char))

		return msgLength_;
	};
};


}