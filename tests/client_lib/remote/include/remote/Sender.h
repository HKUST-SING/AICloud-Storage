/*
 * This file define sender
 *
 * Sender is uesed to sned message
 * to remote servers
 */

#pragma once

/**
 * Standard lib
 */
#include <map>

/** 
 * Internal lib
 */
#include "remote/Message.h"
#include "remote/SecuritySocket.h"

/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/function.hpp>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

namespace singaistorageipc{

class Sender{
public:
	virtual ~Sender() = default;
	virtual int send(std::shared_ptr<Request>,
		boost::function<void(boost::system::error_code const&,std::size_t)>) = 0;
};

class RESTSender : public Sender{

public:
	RESTSender() = delete;
	
	explicit RESTSender(SecuritySocket* socket)
		:socket_(socket){}
	RESTSender(RESTSender&& other)
        :socket_(other.socket_),
         reqMap_(std::move(other.reqMap_))
	{}

	~RESTSender(){
		socket_ = nullptr;
		reqMap_.clear();
	}

	int send(std::shared_ptr<Request>
		,boost::function<void(boost::system::error_code const&
			,std::size_t)>) override;

private:
	//std::shared_ptr<tcp::socket> socket_;
	SecuritySocket* socket_;
	std::map<uint32_t,http::request<http::string_body>> reqMap_;

	void sendCallback(uint32_t id,
			  boost::function<void(
				boost::system::error_code const&,std::size_t)>
					callback,
			  boost::system::error_code const& ec,
			  std::size_t size){
		reqMap_.erase(id);
		callback(ec,size);
	}
};

}
