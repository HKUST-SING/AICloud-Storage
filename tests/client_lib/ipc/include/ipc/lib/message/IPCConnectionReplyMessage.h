/**
 * This message is used for server responding authentication.
 */
#pragma once

#include <cstring>
#include <malloc.h>

#include <folly/io/IOBuf.h>

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCConnectionReplyMessage : public IPCMessage{
public:
	IPCConnectionReplyMessage(){
		msgType_ = IPCMessage::MessageType::CON_REPLY;
		wBufferAddr_ = 0;
		wBufferSize_ = 0;
		rBufferAddr_ = 0;
		rBufferSize_ = 0;
	};

	bool parse(std::unique_ptr<folly::IOBuf> msg) override{
		auto data = msg.get()->data();
		auto length = msg.get()->length();

		IPCMessage::MessageType *msgtype = (IPCMessage::MessageType *)data;
		if(*msgtype != IPCMessage::MessageType::CON_REPLY){
			return false;
		}

		data = parseHead(data);

		if(length != msgLength_){
			return false;
		}

		memcpy(&wBufferAddr_,data,sizeof(uint64_t));

		data += sizeof(uint64_t);
		memcpy(&wBufferSize_,data,sizeof(uint32_t));

		data += sizeof(uint32_t);
		memcpy(&rBufferAddr_,data,sizeof(uint64_t));

		data += sizeof(uint64_t);
		memcpy(&rBufferSize_,data,sizeof(uint32_t));

		data += sizeof(uint32_t);
		memcpy(wBufferName_,data,32*sizeof(char));

		data += sizeof(char) * 32;
		memcpy(rBufferName_,data,32*sizeof(char));

		return true;
	};

	std::unique_ptr<folly::IOBuf> createMsg() override{	
		char *buffer = (char*)malloc(computeLength());
		char *tmp = buffer;

		tmp = createMsgHead(tmp);

		memcpy(tmp,&wBufferAddr_,sizeof(uint64_t));
		tmp += sizeof(uint64_t);

		memcpy(tmp,&wBufferSize_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,&rBufferAddr_,sizeof(uint64_t));
		tmp += sizeof(uint64_t);

		memcpy(tmp,&rBufferSize_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,wBufferName_,32*sizeof(char));
		tmp += 32*sizeof(char);

		memcpy(tmp,rBufferName_,32*sizeof(char));

		auto iobuf = folly::IOBuf::copyBuffer(buffer,msgLength_);

		return std::move(iobuf);
	};


	void setWriteBufferAddress(uint64_t addr){
		wBufferAddr_ = addr;
	};

	void setReadBufferAddress(uint64_t addr){
		rBufferAddr_ = addr;
	};

	void setWriteBufferSize(uint32_t size){
		wBufferSize_ = size;
	};

	void setReadBufferSize(uint32_t size){
		rBufferSize_ = size;
	};

	void setWriteBufferName(const char *val){
		memcpy(wBufferName_,val,32*sizeof(char));
	};

	void setReadBufferName(const char *val){
		memcpy(rBufferName_,val,32*sizeof(char));
	};

	uint64_t getWriteBufferAddress(){
		return wBufferAddr_;
	};

	uint64_t getReadBufferAddress(){
		return rBufferAddr_;
	};

	uint32_t getWriteBufferSize(){
		return wBufferSize_;
	}

	uint32_t getReadBufferSize(){
		return rBufferSize_;
	}

	void getWriteBufferName(char *buf){
		memcpy(buf,wBufferName_,32*sizeof(char));
	};

	void getReadBufferName(char *buf){
		memcpy(buf,rBufferName_,32*sizeof(char));
	};

private:
	uint64_t wBufferAddr_;
	uint32_t wBufferSize_;
	uint64_t rBufferAddr_;
	uint32_t rBufferSize_;
	char wBufferName_[32];
	char rBufferName_[32];

	uint32_t computeLength() override{
		msgLength_ = computeHeadLength()
					+ sizeof(uint64_t)
					+ sizeof(uint32_t)
					+ sizeof(uint64_t)
					+ sizeof(uint32_t)
					+ 32*sizeof(char)
					+ 32*sizeof(char);
		return msgLength_;
	};
};

}
