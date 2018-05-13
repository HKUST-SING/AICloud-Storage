
/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/bind.hpp>

/**
 * Internal lib
 */
#include "remote/Message.h"
#include "remote/Sender.h"
#include "include/CommonCode.h"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

namespace singaistorageipc{

int
RESTSender::send(std::shared_ptr<Request> request
		,boost::function<void(boost::system::error_code const&
				     ,std::size_t)> callback)
{
	if(socket_->getSocket().is_open()){
		return -2;
	}

	http::verb verb;
	std::string target;
	/**
	 * HTTP/1.1
	 */
	int version = 11;

	/**
 	 * Set the target
	 */
	target = request->objectPath_;

	/**
	 * Retrive op type
	 */	
	switch(request->opType_){
	case CommonCode::IOOpCode::OP_READ:
		verb = http::verb::get;
		break;
	case CommonCode::IOOpCode::OP_CHECK_WRITE:
		verb = http::verb::put;
		break;
	case CommonCode::IOOpCode::OP_DELETE:
		verb = http::verb::delete_;
		break;
	case CommonCode::IOOpCode::OP_COMMIT:
		verb = http::verb::post;
		break;
	case CommonCode::IOOpCode::OP_AUTH:
		verb = http::verb::get;
		target = "/auth";
		break;
	default:
		return -1;
	}
	/**
	 * Create HTTP request
	 */
	http::request<http::string_body> req{verb,target,version};
	
	// Host
	std::string host = socket_->getSocket().remote_endpoint().address().to_string();
	host += ":";
	host += std::to_string(socket_->getSocket().remote_endpoint().port());
	req.set(http::field::host,host);

	// Standard head field
	switch(request->opType_){
	case CommonCode::IOOpCode::OP_AUTH:
		// TODO: set the time larger than heart beat
		req.set(http::field::keep_alive,1000);
		break;
	case CommonCode::IOOpCode::OP_COMMIT:{
		std::string type;
		switch(request->msgEnc_){
		case Message::MessageEncoding::JSON_ENC:
			type = "application/json;charset=utf-8";
			break;
		case Message::MessageEncoding::ASCII_ENC:
			type = "ascii;charset=utf-8";
			break;
		case Message::MessageEncoding::BYTES_ENC:
			type = "btyes;charset=utf-8";
			break;
		default:
			type = "charset=utf-8";
			break;
		}
		req.set(http::field::content_type,type);
		req.body() = (char*)request->data_->data();
		break;
		}
	default:
		break;
	}

	// Accept
	req.set(http::field::accept,"application/json");
	req.set(http::field::accept_charset,"utf-8");

	// Customized field
	req.set(RESTHeadField::USER_ID,request->userID_);
	req.set(RESTHeadField::PASSWD,request->password_);
	req.set(RESTHeadField::TRANSACTION_ID,request->tranID_);	

	req.prepare_payload();

	/**
	 * send the http request
	 */
	reqMap_[request->tranID_] = std::move(req);
	http::async_write(socket_->getSocket()., reqMap_[request->tranID_], 
		boost::bind(&RESTSender::sendCallback,this,request->tranID_,callback,_1,_2));
	return 0;
}

}
