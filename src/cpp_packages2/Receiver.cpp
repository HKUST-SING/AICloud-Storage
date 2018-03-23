
/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/thread/mutex.hpp>

/**
 * Internal lib
 */
#include "Receiver.h"
#include "Message.h"


namespace singaistorageipc{

std::unqiue_ptr<Message>
msgParse(){
	return nullptr;
}

void
poolInsert(std::unqiue_ptr<Message> msg){
	boost::mutex::scoped_lock lock(mutex_);
	receivePool_->push_back(std::move(msg));
}

void
receive(){
	boost::beast::http::response<
			boost::beast::http::dynamic_body> newresponse;
	response_ = newresponse;
	http::async_read(socket_,buffer_,response_,&onRead);
}

void
onRead(boost::system::error_code const& er, std::size_t size){
	if(er.value() == 0){
		/**
		 * successful reception
		 */
		poolInsert(std::move(msgParse()));	
	}

	receive();
}

}