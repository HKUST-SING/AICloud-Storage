/*
 * Define the message structure used in IPC
 */

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
		STATUS = 0;
		AUTH = 1;
		READ = 2;
		WRITE = 3;
		CON_REPLY = 4
	};

	virtual uint32_t computeLength();

	/**
	 * Parse a message. If the message don't meet the type, just return false.
	 *
	 * parse() and createMsg() should call computeLength().
	 */
	virtual bool parse(std::unique_ptr<folly::IOBuf>);

	virtual std::unique_ptr<folly::IOBuf> createMsg();

	MessageType getType(){
		return msgType_;
	};

	uint32_t getLength(){
		return msgLength_;
	};

private:
	MessageType msgType_;

	uint32_t msgLength_;
};

}