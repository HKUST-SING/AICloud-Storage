/**
 * This message is used for server responding error in 
 * authentication or operation.
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include <lib/message/IPCMessage.h>

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

		data += sizeof(IPCMessage::MessageType);
		memcpy(&msgLength_,data,sizeof(uint32_t));

		if(length != msgLength_){
			return false;
		}

		data += sizeof(uint32_t);
		memcpy(&statusType_,data,sizeof(uint16_t));

		data += sizeof(uint16_t);
		memcpy(&opCode_,data,sizeof(uint16_t));

		data += sizeof(uint16_t);
		auto tail = msg.get()->tail();
		pathVal_ = std::string((char*)data,tail-data);

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		void *buffer = (void*)malloc(computeLength());
		void *tmp = buffer;

		memcpy(tmp,&msgType_,sizeof(IPCMessage::MessageType));
		tmp += sizeof(IPCMessage::MessageType);

		memcpy(tmp,&msgLength_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,&statusType_,sizeof(uint16_t));
		tmp += sizeof(uint16_t);

		memcpy(tmp,&opCode_,sizeof(uint16_t));
		tmp += sizeof(uint16_t);

		memcpy(tmp,pathVal_.c_str(),pathVal_.length() * sizeof(char));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};

	void setStatusType(uint16_t type){
		statusType_ = type;
	};

	void setOpCode(uint16_t op){
		opCode_ = op;
	};

	void setPath(const std::string& path){
		pathVal_ = path;
	};

	uint16_t getStatusType(){
		return statusType_;
	};

	uint16_t getOpCode(){
		return opCode_;
	};

	std::string getPath(){
		return pathVal_;
	};

private:
	uint16_t statusType_;
	uint16_t opCode_;
	std::string pathVal_;

	uint32_t computeLength() override{
		msgLength_ = sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t)
					+ sizeof(uint16_t)
					+ sizeof(uint16_t)
					+ (pathVal_.length() * sizeof(char));

		return msgLength_;
	};
};

}
