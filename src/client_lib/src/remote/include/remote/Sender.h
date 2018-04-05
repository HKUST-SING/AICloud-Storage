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
#include <map>

/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/function.hpp>
#include <folly/dynamic.h>

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

namespace singaistorageipc{

class Sender{
public:
	virtual ~Sender() = default;
	virtual int send(folly::dynamic,
		boost::function<void(boost::system::error_code const&,std::size_t)>) = 0;
};

class RESTSender : public Sender{

public:
	RESTSender() = delete;
	
	explicit RESTSender(std::shared_ptr<tcp::socket> socket)
		:socket_(socket){}

	~RESTSender(){
		socket_ = nullptr;
		reqMap_.clear();
	}

	int send(folly::dynamic
			,boost::function<void(boost::system::error_code const&
				,std::size_t)>) override;

private:
	std::shared_ptr<tcp::socket> socket_;
	std::map<uint32_t,http::request<http::string_body>> reqMap_;

	void sendCallback(uint32_t id,
					boost::function<void(
						boost::system::error_code const&,std::size_t)> callback,
					boost::system::error_code const& ec,
					std::size_t size){
		reqMap_.erase(id);
		callback(ec,size);
	}
};

}
