
/**
 * Standard lib
 */
#include <thread>

/**
 * External lib
 */
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/system/system_error.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <folly/io/async/AsyncSocketException.h>

/**
 * Internal lib
 */
#include "remote/Security.h"
#include "remote/ServerChannel.h"
#include "remote/Sender.h"
#include "remote/ChannelContext.h"

using tcp = boost::asio::ip::tcp;

namespace singaistorageipc{

bool
ServerChannel::initChannel()
{
	tcp::endpoint ep(boost::asio::ip::address::from_string(
							cxt_.remoteServerAddress_.c_str()), cxt_.port_);	
	LOG(INFO) << "start to initial channel";
	try{
		socket_->connect(ep);
	}
	catch(boost::system::system_error & err){
		LOG(WARNING) << "fail to initial channel"
					 <<" due to error in socket connection";
		return false;
	}

	/**
	 * Start the socket
	 * Now we can send requests and receive request
	 * through this socket
	 */
	std::thread tmpthread([&]{
			restReceiver_.startReceive();
			do{
				restReceiver_.continue_ = false;
				ioc_->run();
			}while(restReceiver_.continue_);});
	socketThread_ = std::move(tmpthread);

	LOG(INFO) << "initial channel successfully";
	return true;
}

void
ServerChannel::closeChannel()
{
	LOG(INFO) << "start to close channel";

	try{	
		socket_->shutdown(tcp::socket::shutdown_both);
		//socket_->close();
		ioc_->stop();
		if(socketThread_.joinable())
			socketThread_.join();
		//socket_->release();
	}
	catch(std::exception& e){
		LOG(ERROR) << "cannot close the socket conneting to the remote server.\n"
				   << "the reason is: " << e.what();
	}

	LOG(INFO) << "finish to close channel";
}

folly::AsyncSocketException::AsyncSocketExceptionType
ServerChannel::fromBoostErrortoFollyException0(
	boost::system::error_code const& er)
{
	folly::AsyncSocketException::AsyncSocketExceptionType type;

	switch(er.value()){
	case boost::system::errc::errc_t::not_connected:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::NOT_OPEN;
		break;
	case boost::system::errc::errc_t::timed_out:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::TIMED_OUT;
		break; 
	case boost::system::errc::errc_t::interrupted:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::INTERRUPTED;
		break;
	case boost::system::errc::errc_t::bad_message:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::CORRUPTED_DATA;
		break;
	case boost::system::errc::errc_t::io_error:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::INTERNAL_ERROR;
		break;
	case boost::system::errc::errc_t::not_supported:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::NOT_SUPPORTED;
		break;
	case boost::system::errc::errc_t::network_down:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::NETWORK_ERROR;
		break;
	default:
		type = folly::AsyncSocketException::AsyncSocketExceptionType::UNKNOWN;	
	}

	return type;
}

bool
ServerChannel::sendMessage(std::shared_ptr<Request> msg
           ,folly::AsyncWriter::WriteCallback* callback)
{
	boost::function<void (boost::system::error_code const&,
						std::size_t)> f
		= [=](boost::system::error_code const& er,std::size_t size){
			if(er.value() == 0){
				callback->writeSuccess();
			}
			else{
				folly::AsyncSocketException::AsyncSocketExceptionType type 
					= this->fromBoostErrortoFollyException0(er);
				folly::AsyncSocketException ex(type,er.message());
				callback->writeErr(size,ex);
			}
		  };
	int r = restSender_.send(msg,f);
	if(r == 0){
		DLOG(INFO) << "send a message";
		return true;
	}
	else{
		DLOG(INFO) << "fail to send a message, error code: " << r;
		return false;
	}
}

std::vector<std::unique_ptr<Response>>
ServerChannel::pollReadMessage()
{
	return std::move(receivePool_->poll());
}

} //namespace
