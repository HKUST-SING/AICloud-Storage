/**
 * This message is used for client authentication
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCAuthenticationMessage : public IPCMessage{
public:
	IPCAuthenticationMessage(){
		msgType_ = IPCMessage::MessageType::AUTH;
	};

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::AUTH){
			return false;
		}

		data = parseHead(data);

		if(length != msgLength_){
			return false;
		}

		memcpy(&usernameLength_,data,sizeof(uint16_t));

		data += sizeof(uint16_t);
		username_ = std::string((char*)data,usernameLength_);

		data += usernameLength_ * sizeof(char);
		memcpy(password_,data,32*sizeof(char));

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		void *buffer = (void*)malloc(computeLength());
		void *tmp = buffer;

		tmp = createMsgHead(tmp);

		memcpy(tmp,&usernameLength_,sizeof(uint16_t));
		tmp += sizeof(uint16_t);

		memcpy(tmp,username_.c_str(),usernameLength_ * sizeof(char));
		tmp += usernameLength_ * sizeof(char);

		memcpy(tmp,password_,32*sizeof(char));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};

	void setUsername(const std::string& name){
		username_ = name;
		usernameLength_ = username_.length() * sizeof(char);
	};

	void setPassword(char *val){
		memcpy(password_,val,32*sizeof(char));
	};

	uint16_t getUsernameLength(){
		return usernameLength_;
	};

	std::string getUsername(){
		return username_;
	};

	void getPassword(char *rval){
		memcpy(rval,password_,32*sizeof(char));
	};

private:
	uint16_t usernameLength_;
	std::string username_;
	char password_[32];

	uint32_t computeLength() override{
		msgLength_ = computeHeadLength()
					+ sizeof(uint16_t)
					+ (usernameLength_ * sizeof(char))
					+ (32 * sizeof(char));
		return msgLength_;
	};
};

}