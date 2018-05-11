#pragma once

// C++ std lib
#include <vector>
#include <memory>
#include <thread>


// Boost lib
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>  


// Facebook folly lib
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocketException.h>


// Project lib
#include "Sender.h"
#include "Receiver.h"
#include "ChannelContext.h"
#include "Message.h"

namespace singaistorageipc
{


class Security; // forward delcaration

class ServerChannel
{

  /**
   * An interface for communication with a remote authentication
   * server. The interface provides APIs for sending/receiving
   * authentication messages to the authentication server in 
   * to ensure that user operations are valid.
   */

    public:

      ServerChannel() = delete;
      ServerChannel(const ServerChannel&) = delete;
      ServerChannel(ServerChannel&& other)
      :ioc_(std::move(other.ioc_)),
       socket_(std::move(other.socket_)),
       restSender_(std::move(other.restSender_)),
       receivePool_(std::move(other.receivePool_)),
       restReceiver_(std::move(other.restReceiver_)),
       cxt_(std::move(other.cxt_)),
       socketThread_(std::move(other.socketThread_))
      {}

      ServerChannel(ChannelContext cxt)
      :ioc_(new boost::asio::io_context),
       socket_(std::make_shared<boost::asio::ip::tcp::socket>(
		std::move(boost::asio::ip::tcp::socket(*ioc_)))),
       restSender_(socket_),
       receivePool_(std::make_shared<ReceivePool>(new ReceivePool())),
       restReceiver_(socket_,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(
          cxt.remoteServerAddress_),cxt.port_),
        receivePool_),
       cxt_(cxt)
      {}

      virtual ~ServerChannel() = default;
      bool initChannel();	
      void closeChannel();

      bool sendMessage(std::shared_ptr<Request> msg
                      , folly::AsyncWriter::WriteCallback* callback);
      std::vector<std::unique_ptr<Response> > pollReadMessage(); 
 
    private:
      std::unique_ptr<boost::asio::io_context> ioc_;
      std::shared_ptr<boost::asio::ip::tcp::socket> socket_;

      RESTSender restSender_;

      /**
       * message received by the receiver
       */
      std::shared_ptr<ReceivePool> receivePool_;
      RESTReceiver restReceiver_;

      ChannelContext cxt_;

      /**
       * this thread receive response from remote server
       */
      std::thread socketThread_;

      /**
       * Helper functions
       */
      folly::AsyncSocketException::AsyncSocketExceptionType 
        fromBoostErrortoFollyException0(boost::system::error_code const& er);

}; // class ServerChannel


} // namespace singaistorageipc
