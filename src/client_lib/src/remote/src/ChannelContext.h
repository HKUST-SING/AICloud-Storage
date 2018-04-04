/*
 * This file define the context
 * used in channel initialization
 */

#pragma once

namespace singaistorageipc{

class ChannelContext{
public:
	std::string remoteServerAddress_;
	unsigned short port_;
};

}