
/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>

/**
 * Internal lib
 */
#include "Receiver.h"
#include "Message.h"

namespace http = boost::beast::http;

namespace singaistorageipc{

std::unique_ptr<Message>
RESTReceiver::msgParse(){
	return nullptr;
}

void
RESTReceiver::poolInsert(std::unique_ptr<Message> msg){
	boost::mutex::scoped_lock lock(*mutex_);
	receivePool_->push_back(std::move(msg));
}

void
RESTReceiver::receive(){
	boost::beast::http::response<
			boost::beast::http::dynamic_body> newresponse;
	response_ = newresponse;
	http::async_read(*socket_,buffer_,response_,
		boost::bind(&RESTReceiver::onRead,this,_1,_2));
}

void
RESTReceiver::onRead(boost::system::error_code const& er, std::size_t size){
	if(er.value() == 0){
		/**
		 * successful reception
		 */
		poolInsert(std::move(msgParse()));	
	}

	receive();
}

}