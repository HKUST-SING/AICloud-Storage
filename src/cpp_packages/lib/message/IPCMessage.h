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

	/**
	 * createMsg() should call computeLength().
	 */
	virtual std::unique_ptr<folly::IOBuf> createMsg() = 0;

	MessageType getType(){
		return msgType_;
	};

	uint32_t getLength(){
		return msgLength_;
	};

protected:
	MessageType msgType_;

	uint32_t msgLength_;
private:
	virtual uint32_t computeLength() = 0;
};

}
