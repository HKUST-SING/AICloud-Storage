#pragma once

// C++ std lib
#include <vector>
#include <memory>
#include <thread>


// Boost lib
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>


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

      ServerChannel() = default;
      ServerChannel(const ServerChannel&) = delete;
      ServerChannel(ServerChannel&& other);
      virtual ~ServerChannel();
      bool initChannel();	
      void closeChannel();

      bool sendMessage(std::shared_ptr<Request> msg
                      , folly::AsyncWriter::WriteCallback* callback);
      std::vector<std::unique_ptr<Response> > pollReadMessage(); 
 
    private:
      boost::asio::io_context ioc_;
      std::shared_ptr<boost::asio::ip::tcp::socket> socket_;

      RESTSender restSender_;
      RESTReceiver restReceiver_;

      ChannelContext cxt_;

      /**
       * this thread receive response from remote server
       */
      std::thread socketThread_;

      /**
       * message received by the receiver
       */
      std::shared_ptr<ReceivePool> receivePool_;

      /**
       * Helper functions
       */
      folly::AsyncSocketException::AsyncSocketExceptionType 
        fromBoostErrortoFollyException0(boost::system::error_code const& er);

}; // class ServerChannel


} // namespace singaistorageipc
