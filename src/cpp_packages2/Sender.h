/*
 * This file define sender
 *
 * Sender is uesed to sned message
 * to remote servers
 */

#pragma once

/**
 * Standar lib
 */
#include <functional>

/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <folly/dynamic.h>

using tcp = boost::asio::ip::tcp;

namespace singaistorageipc{

class Sender{
public:
	virtual ~Sender() = default;
	virtual int send(folly::dynamic,
		std::function<void(boost::system::error_code const&,std::size_t)>) = 0;
};

class RESTSender : public Sender{

public:
	RESTSender() = delete;
	
	explicit RESTSender(std::shared_ptr<tcp::socket> socket)
		:socket_(socket){}

	~RESTSender(){
		
	}

	int send(folly::dynamic
			,std::function<void(boost::system::error_code const&
				,std::size_t)>) override;

private:
	std::shared_ptr<tcp::socket> socket_;

};

}