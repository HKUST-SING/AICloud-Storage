
/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <folly/io/IOBuf.h>

/**
 * Internal lib
 */
#include "remote/Receiver.h"
#include "remote/Message.h"
#include "include/CommonCode.h"

namespace http = boost::beast::http;

namespace singaistorageipc{

std::unique_ptr<Response>
RESTReceiver::msgParse(){
	std::unique_ptr<Response> response = nullptr;
	try{
		/**
		 * Check the status code in response message
		 */
		if(response_.result() == http::status::ok){
			response.reset(new Response(
					std::stoul(response_.at(RESTHeadField::TRANSACTION_ID).data()
					  ,nullptr,10)
					,CommonCode::IOStatus::STAT_SUCCESS));
		}
		else{
			response.reset(new Response(	
					std::stoul(response_.at(RESTHeadField::TRANSACTION_ID).data()
					  ,nullptr,10)
					,CommonCode::IOStatus::ERR_INTERNAL));
		}
		response->data_ = folly::IOBuf::copyBuffer(
							response_.body().data(),response_.body().length());
	
		std::string contenttype = "";
		try{
		  contenttype = response_.at(http::field::content_type).data();
		}
		catch(std::out_of_range){
		  contenttype = "";
		}
		if(contenttype.find("json") != std::string::npos){
			response->msgEnc_ = Message::MessageEncoding::JSON_ENC;
		}
		else if(contenttype.find("ascii") != std::string::npos){
			response->msgEnc_ = Message::MessageEncoding::ASCII_ENC;
		}
		else if(contenttype.find("bytes") != std::string::npos){
		  	response->msgEnc_ = Message::MessageEncoding::BYTES_ENC;
		}
		else{
			response->msgEnc_ = Message::MessageEncoding::NONE_ENC;
		}

		return response;
	} // try
	catch(std::out_of_range){
	  return nullptr;
	}
}

void
RESTReceiver::poolInsert(std::unique_ptr<Response> msg){
	receivePool_->insert(std::move(msg));
}

void
RESTReceiver::receive(){
	boost::beast::http::response<
			boost::beast::http::string_body> newresponse;
	response_ = std::move(newresponse);
	http::async_read(*socket_,buffer_,response_,
		boost::bind(&RESTReceiver::onRead,this,_1,_2));
}

void
RESTReceiver::onRead(boost::system::error_code const& er, std::size_t size){
	DLOG(INFO) << "receive a response from remote server";
	continue_ = false;
	if(er.value() == 0){
		/**
		 * successful reception
		 */
		poolInsert(std::move(msgParse()));	
		DLOG(INFO) << "insert the response into the pool";
	}
	else if(er.value() == 1){
		/**
		 * That means socket disconnected.
		 * Connects again.
		 */
		socket_->get_io_context().stop();
		try{
			socket_->shutdown(
				boost::asio::ip::tcp::socket::shutdown_both);
			socket_->close();
			socket_->connect(ep_);
		}
		catch(boost::system::system_error &er){
			LOG(FATAL) << "Fail to reconnect to remote socket\n"
					   << er.what();
		}
		socket_->get_io_context().restart();
		continue_ = true;
	}

	receive();
}

}
