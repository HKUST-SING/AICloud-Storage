
/**
 * External lib
 */
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/address.hpp>
#include <folly/dynamic.h>
#include <folly/DynamicConverter.h>
#include <folly/json.h>

/**
 * Internal lib
 */
#include "Message.h"
#include "Sender.h"

using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;

namespace singaistorageipc{
const std::string MessageField::OP_TYPE;
const std::string MessageField::PATH;
const std::string MessageField::USER_ID;
const std::string MessageField::PASSWD;

int
RESTSender::send(folly::dynamic map
				,std::function<void(boost::system::error_code const&
								,std::size_t)> callback){
	if(!socket_->is_open()){
		return -2;
	}

	http::verb verb;
	std::string target;
	/**
	 * HTTP/1.1
	 */
	int version = 11;

	/**
	 * Retrive op type
	 */
	auto kv = map.find(MessageField::OP_TYPE);
	if(kv == map.items().end()){
		/**
		 * Haven't set op type, authentication request
		 */
		verb = http::verb::get;
		target = "/auth";
	}
	else{
		/**
		 * Determine the type of request
		 */
		switch(folly::convertTo<uint8_t>(kv->second)){
			case 0:
				verb = http::verb::get; // READ
				break;
			case 1:
				verb = http::verb::put; // WRIET
				break;
			case 2:
				verb = http::verb::delete_; // DELETE
				break;
			default:
				/**
				 * Unknown type
				 */
				return -1;
		}
		map.erase(MessageField::OP_TYPE);
		/**
		 * Retrive object path
		 */
		target = map[MessageField::PATH].asString();
	}
	map.erase(MessageField::PATH);
	/**
	 * Create HTTP request
	 */
	http::request<http::string_body> req{verb,target,version};
	std::string host = socket_->remote_endpoint().address().to_string();
	host += std::to_string(socket_->remote_endpoint().port());
	req.set(http::field::host,host);
	req.set(http::field::accept,"application/json");
	req.set(http::field::accept_charset,"utf-8");

	if(verb == http::verb::get){
		/**
	 	* TODO: keep alive field
	 	*/
	 	//req.set(http::field::keep_alive,std::to_string(life_time));
	}

	std::string authorization = "Basic ";
	authorization += map[MessageField::USER_ID].asString() 
				  + ":" + map[MessageField::PASSWD].asString();
	req.set(http::field::authorization,authorization);
	map.erase(MessageField::USER_ID);
	map.erase(MessageField::PASSWD);

	req.body() = folly::toJson(std::move(map));
	req.prepare_payload();

	/**
	 * send the http request
	 */
	http::async_write(*socket_, req, std::move(callback));
	return 0;
}

}