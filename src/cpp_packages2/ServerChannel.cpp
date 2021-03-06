
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

/**
 * Internal lib
 */
#include "ServerChannel.h"
#include "Sender.h"
#include "ChannelContext.h"

using tcp = boost::asio::ip::tcp;

namespace singaistorageipc{

bool
ServerChannel::initChannel(){
	tcp::endpoint ep(boost::asio::ip::address::from_string(
							cxt_.remoteServerAddress_.c_str()), cxt_.port_);	
	try{
		socket_->connect(ep);
	}
	catch(boost::system::system_error & err){
		return false;
	}

	/**
	 * Start the socket
	 * Now we can send requests and receive request
	 * through this socket
	 */
	std::thread tmpthread([&]{
			restReceiver_.startReceive();
			ioc_.run();});
	socketThread_ = std::move(tmpthread);

	return true;
}

} //namespace