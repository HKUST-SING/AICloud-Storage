/**
 * This message is used for client write request and server read response
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCWriteRequestMessage : public IPCMessage{
public:
	IPCWriteRequestMessage(){
		msgType_ = IPCMessage::MessageType::WRITE;
		pathLength_ = 0;
		properties_ = 0;
		staringAddr_= 0;
		dataLength_ = 0;
	}

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::WRITE){
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

		data += pathLength_ * sizeof(char);
		memcpy(&properties_,data,sizeof(uint32_t));

		data += sizeof(uint32_t);
		memcpy(&staringAddr_,data,sizeof(uint64_t));

		data += sizeof(uint64_t);
		memcpy(&dataLength_,data,sizeof(uint64_t));

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
		tmp += pathLength_ * sizeof(char);

		memcpy(tmp,&properties_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,&staringAddr_,sizeof(uint64_t));
		tmp += sizeof(uint64_t);

		memcpy(tmp,&dataLength_,sizeof(uint64_t));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};


	void setPath(const std::string& val){
		pathVal_ = val;
		pathLength_ = pathVal_.length() * sizeof(char);
	};

	void setProperties(uint32_t p){
		properties_ = p;
	};

	void setStartAddress(uint64_t addr){
		staringAddr_ = addr;
	}

	void setDataLength(uint64_t length){
		dataLength_ = length;
	}

	uint16_t getPathLength(){
		return pathLength_;
	};

	std::string getPath(){
		return pathVal_;
	};

	uint32_t getProperties(){
		return properties_;
	};

	uint64_t getStartingAddress(){
		return staringAddr_;
	};

	uint64_t getDataLength(){
		return dataLength_;
	};

private:
	uint16_t pathLength_;
	std::string pathVal_;
	uint32_t properties_;
	uint64_t staringAddr_;
	uint64_t dataLength_;

	uint32_t computeLength() override{
		msgLength_ = sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t)
					+ sizeof(uint16_t)
					+ (pathLength_ * sizeof(char))
					+ sizeof(uint32_t);
					+ sizeof(uint64_t);
					+ sizeof(uint64_t);
		return msgLength_;
	};
};

}