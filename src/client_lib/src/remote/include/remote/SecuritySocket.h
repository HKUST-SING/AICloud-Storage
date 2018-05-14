#pragma once

// Boost lib
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_context.hpp>

namespace singaistorageipc{

class SecuritySocket{

public:
	SecuritySocket(const std::string& address,
                       const unsigned short port,
		       boost::asio::io_context& ioc)
	:ep_(boost::asio::ip::address::from_string(
          address),port),
	 socket_(std::make_unique<boost::asio::ip::tcp::socket>(
		ioc))
	{}

	SecuritySocket(SecuritySocket&& other)
	:ep_(std::move(other.ep_)),
	 socket_(std::move(other.socket_))
	{}

	bool initialize();

	void close();

	inline boost::asio::ip::tcp::socket& getSocket()
	{
		return *socket_;
	}

	void reconnect();

private:	
boost::asio::ip::tcp::endpoint ep_;
std::unique_ptr<boost::asio::ip::tcp::socket> socket_;

};

} // namesapce singaistorage
