#include "remote/SecuritySocket.h"

#include <boost/system/system_error.hpp>
#include <glog/logging.h>

namespace singaistorageipc{

bool
SecuritySocket::initialize()
{
	try{
		socket_->connect(ep_);
	}
	catch(boost::system::system_error &er){
		LOG(ERROR) << "Fail to reconnect to remote socket\n"
				   << er.what();
		return false;
	}
	return true;
}

void 
SecuritySocket::reconnect()
{
	try{
		socket_.reset(new boost::asio::ip::tcp::socket(
				socket_->get_io_context()));
		socket_->connect(ep_);
	}
	catch(boost::system::system_error &er){
		LOG(FATAL) << "Fail to reconnect to remote socket\n"
				   << er.what();
	}
}

void
SecuritySocket::close()
{
	try{
		socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	}
	catch(std::exception& e){
		LOG(ERROR) << "cannot close the socket conneting to the remote server.\n"
				   << "the reason is: " << e.what();
	}
}

}
