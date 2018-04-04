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
#include <folly/dynamic.h>

/**
 * Internal lib
 */
#include "Message.h"

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
		std::shared_ptr<std::vector<std::unique_ptr<Message>>> receivePool, 
		std::shared_ptr<boost::mutex> mutex)
		:socket_(socket),receivePool_(receivePool),mutex_(mutex){}

	~RESTReceiver() = default;

	void startReceive() override{
		receive();
	}
	void receive();


private:
	std::shared_ptr<tcp::socket> socket_;
	std::shared_ptr<
		std::vector<std::unique_ptr<Message>>> receivePool_;
	std::shared_ptr<boost::mutex> mutex_;

	/**
	 * `buffer_`, `response_` and `readHandler_`
	 * are used in receiving response
	 */
	boost::beast::flat_buffer buffer_;
	boost::beast::http::response<
			boost::beast::http::dynamic_body> response_;

	/**
	 * Reception callback
	 */
	void onRead(boost::system::error_code const&,std::size_t);
	/**
	 * Rebuild the message
	 */
	std::unique_ptr<Message> msgParse();
	/**
	 * Insert receiving message into the pool
	 */
	void poolInsert(std::unique_ptr<Message> msg);

};

}