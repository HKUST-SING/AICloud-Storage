/*
 * This file define sender
 *
 * Sender is uesed to sned message
 * to remote servers
 */

#pragma once

/**
 * External lib
 */
#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <folly/dynamic.h>
#include <folly/Function.h>

using tcp = boost::asio::ip::tcp;

namespace singaistorageipc{

class Sender{
public:
	virtual ~Sender() = default;
	virtual int send(folly::dynamic,
		folly::Function<void(boost::system::error_code const&,std::size_t)>) = 0;
};

class RESTSender : public Sender{

public:
	RESTSender(boost::asio::io_context ioc):socket_(ioc){};
	
	explicit RESTSender(tcp::socket&& socket)
		:socket_(std::move(socket)){}

	~RESTSender(){
		closeSocket();
	}

	void connectServer(tcp::endpoint ep){
		socket_.connect(ep);
	}

	void closeSocket(){
		if(socket_.is_open()){
			socket_.close();
		}
	}

	int send(folly::dynamic
			,folly::Function<void(boost::system::error_code const&
				,std::size_t)>) override;

private:
	tcp::socket socket_;
};

}