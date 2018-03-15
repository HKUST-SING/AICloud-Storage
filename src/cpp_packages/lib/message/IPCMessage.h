/*
 * Define the message structure used in IPC
 */
#pragma once

#include <folly/io/IOBuf.h>

namespace singaistorageipc{

/**
 * All lengthes in message are counted in bytes.
 *
 * All passwords are encrypted by hash function.
 * So they have fix length.
 *
 * All properties are bitmap.
 */
class IPCMessage{
public:
	enum class MessageType : uint8_t{
		STATUS = 0,
		AUTH = 1,
		READ = 2,
		WRITE = 3,
		CON_REPLY = 4,
		CLOSE = 5,
		DELETE = 6
	};

	/**
	 * Parse a message. If the message don't meet the type, just return false.
	 */
	virtual bool parse(std::unique_ptr<folly::IOBuf>) = 0;

	const uint8_t* parseHead(const uint8_t *data){
		data += sizeof(IPCMessage::MessageType);
		memcpy(&msgID_,data,sizeof(uint32_t));

		data += sizeof(uint32_t);
		memcpy(&msgLength_,data,sizeof(uint32_t));

		data += sizeof(uint32_t);
		return data;
	}

	/**
	 * createMsg() should call computeLength().
	 */
	virtual std::unique_ptr<folly::IOBuf> createMsg() = 0;

	void* createMsgHead(void *tmp){
		memcpy(tmp,&msgType_,sizeof(IPCMessage::MessageType));
		tmp += sizeof(IPCMessage::MessageType);

		memcpy(tmp,&msgID_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		memcpy(tmp,&msgLength_,sizeof(uint32_t));
		tmp += sizeof(uint32_t);

		return tmp;
	}

	MessageType getType(){
		return msgType_;
	};

	uint32_t getID(){
		return msgID_;
	}

	void setID(uint32_t id){
		msgID_ = id;
	}

	uint32_t getLength(){
		return msgLength_;
	};

protected:
	MessageType msgType_;

	uint32_t msgID_;

	uint32_t msgLength_;

	uint32_t computeHeadLength(){
		return sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t)
					+ sizeof(uint32_t);
	}
private:
	virtual uint32_t computeLength() = 0;
};

}
