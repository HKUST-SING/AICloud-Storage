/*
 * This file define the context
 * used in channel initialization
 */

#pragma once

namespace singaistorageipc{

class ChannelContext{
public:
	ChannelContext() = default;
	ChannelContext(const ChannelContext& other)
	:remoteServerAddress_(other.remoteServerAddress_),
	 port_(other.port_)
	{}

	ChannelContext(ChannelContext&& other)
	:remoteServerAddress_(other.remoteServerAddress_),
	 port_(other.port_)
	{}

	std::string remoteServerAddress_;
	unsigned short port_;
};

}
