#pragma once

// C++ std lib
#include <vector>
#include <memory>


// Facebook folly lib
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/AsyncSocketException.h>


// Project lib

namespace singaistorageipc
{

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
          explicit ChannnelWriteCallback(std::shared_ptr<Security> sec)
          : secure_(sec),
            msg_(nullptr)
          {}
           
          ChannelWriteCallbakc(std::shared_ptr<Security> sec,
                               std::shared_ptr<Message> msg)
          : secure_(sec),
            msg_(msg)
          {}

          ChannelWriteCallback(const ChannelWriteCallbakc&) = delete;
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

      bool sendMessage(std::shared_ptr<Message> msg, folly::AsyncWriter::WriteCallback* callback);
      std::vector<std::unique_ptr<Message> > pollReadMessage(); 
 

       

   
    



}; // class ServerChannel


} // namespace singaistorageipc
