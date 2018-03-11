/**
 * This message is used for client authentication
 */
#pragma once

#include "IPCMessage.h"

namespace singaistorageipc{

class IPCAuthenticationMessage : public IPCMessage{
public:
	uint32_t computeLength() override{
		msgLength_ = sizeof(IPCMessage::MessageType)
					+ sizeof(uint32_t)
					+ sizeof(uint16_t)
					+ (username_.size() * sizeof(char))
					+ (32 * sizeof(char));
		msgType_ = IPCMessage::MessageType::AUTH;
	};

private:
	uint16_t usernameLength_;
	std::string username_;
	char[32] password_;
};

}