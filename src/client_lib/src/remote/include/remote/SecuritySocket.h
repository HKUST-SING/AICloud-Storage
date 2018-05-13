#pragma once

// Boost lib
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_context.hpp>


namespace singaistorage{

class SecuritySocket{

public:
	SecuritySocket(std::string address,unsigned short port,
		boost::asio::io_context& ioc)
	:ep_(boost::asio::ip::address::from_string(
          address),port),
	 socket_(new boost::asio::ip::tcp::socket(ioc))
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

}