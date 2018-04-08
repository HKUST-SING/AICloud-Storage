#pragma once

// C++ std lib
#include <vector>
#include <memory>
#include <thread>


// Boost lib
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/system/system_error.hpp>


// Facebook folly lib
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocketException.h>


// Project lib
#include "Sender.h"
#include "Receiver.h"
#include "ChannelContext.h"

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

      /** Write callback for notifying if a message is 
       *  successfully written.
       *
       */
      class ChannelWriteCallback: public folly::AsyncWriter::WriteCallback
      {
        public:
          explicit ChannelWriteCallback(std::shared_ptr<Security> sec)
          : secure_{sec},
            msg_(nullptr)
          {}
           
          ChannelWriteCallback(std::shared_ptr<Security> sec,
                               std::shared_ptr<Message> msg)
          : secure_{sec},
            msg_(msg)
          {}

          ChannelWriteCallback(const ChannelWriteCallback&) = delete;
          ChannelWriteCallback() = delete;

          ChannelWriteCallback(ChannelWriteCallback&& other)
          : secure_(std::move(other.secure_)),
            msg_(std::move(other.msg_))
          {}


          void writeSuccess() noexcept override
          {}

          void writeErr(size_t bytesWritten, 
                        const folly::AsyncSocketException& ex) 
                                             noexcept override
          {}
          


        private:
          std::shared_ptr<Security> secure_;
          std::shared_ptr<Message>  msg_;


          
      }; // class ChannelWriteCallback
	 

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
      std::shared_ptr<
        std::vector<std::unique_ptr<Response>>> receivePool_;
      std::shared_ptr<boost::mutex> mutex_;

      /**
       * Helper functions
       */
      folly::AsyncSocketException::AsyncSocketExceptionType 
        fromBoostErrortoFollyException0(boost::system::error_code const& er);

}; // class ServerChannel


} // namespace singaistorageipc
