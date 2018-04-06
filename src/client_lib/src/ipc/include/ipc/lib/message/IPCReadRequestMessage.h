/**
 * This message is used for client read request and server write response.
 */

#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCReadRequestMessage : public IPCMessage{
public:
	IPCReadRequestMessage(){
		msgType_ = IPCMessage::MessageType::READ;
		pathLength_ = 0;
		properties_ = 0;
	};

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::READ){
			return false;
		}

		data = parseHead(data);

		if(length != msgLength_){
			return false;
		}

		memcpy(&pathLength_,data,sizeof(uint16_t));

		data += sizeof(uint16_t);
		pathVal_ = std::string((char*)data,pathLength_);

		data += pathLength_ * sizeof(char);
		memcpy(&properties_,data,sizeof(uint32_t));

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		char *buffer = (char*)malloc(computeLength());
		char *tmp = buffer;

		tmp = createMsgHead(tmp);

		memcpy(tmp,&pathLength_,sizeof(uint16_t));
		tmp += sizeof(uint16_t);

		memcpy(tmp,pathVal_.c_str(),pathLength_ * sizeof(char));
		tmp += pathLength_ * sizeof(char);

		memcpy(tmp,&properties_,sizeof(uint32_t));

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

	uint16_t getPathLength(){
		return pathLength_;
	};

	std::string getPath(){
		return pathVal_;
	};

	uint32_t getProperties(){
		return properties_;
	};

private:
	uint16_t pathLength_;
	std::string pathVal_;
	uint32_t properties_;

	uint32_t computeLength() override{
		msgLength_ = computeHeadLength()
					+ sizeof(uint16_t)
					+ (pathLength_ * sizeof(char))
					+ sizeof(uint32_t);
		return msgLength_;
	};
};

}
