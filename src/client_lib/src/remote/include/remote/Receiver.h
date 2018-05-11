/*
 * This file define receiver
 *
 * Receiver is used to receive message
 * from the remote server
 */
#pragma once

/**
 * Standar lib
 */
#include <vector>

/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread/mutex.hpp>

/**
 * Internal lib
 */
#include "remote/Message.h"

using tcp = boost::asio::ip::tcp;

namespace singaistorageipc{

class Receiver{
public:
	virtual ~Receiver() = default;
	virtual void startReceive() = 0;
};

class RESTReceiver : public Receiver{
public:
	RESTReceiver() = delete;
	explicit RESTReceiver(std::shared_ptr<tcp::socket> socket, 
		tcp::endpoint ep,
		std::shared_ptr<ReceivePool> receivePool)
		:socket_(socket),ep_(ep),receivePool_(receivePool){}
	RESTReceiver(RESTReceiver&& other)
	:socket_(std::move(other.socket_)),
	 ep_(std::move(other.ep_)),
	 receivePool_(std::move(other.receivePool_))
	{}

	~RESTReceiver() = default;

	void startReceive() override{
		receive();
	}
	void receive();

	bool continue_{false};
private:
	std::shared_ptr<tcp::socket> socket_;
	tcp::endpoint ep_;

	std::shared_ptr<ReceivePool> receivePool_;

	/**
	 * `buffer_`, `response_` and `readHandler_`
	 * are used in receiving response
	 */
	boost::beast::flat_buffer buffer_;
	boost::beast::http::response<
			boost::beast::http::string_body> response_;

	/**
	 * Reception callback
	 */
	void onRead(boost::system::error_code const&,std::size_t);
	/**
	 * Rebuild the message
	 */
	std::unique_ptr<Response> msgParse();
	/**
	 * Insert receiving message into the pool
	 */
	void poolInsert(std::unique_ptr<Response> msg);

};

}
