/*
 * This file defines the message structure.
 */
#pragma once

/**
 * External lib
 */
#include <folly/io/IOBuf.h>
#include <boost/thread/mutex.hpp>

/**
 * Internal lib
 */
#include "include/CommonCode.h"

namespace singaistorageipc{

class RESTHeadField{
public:
	static const std::string TRANSACTION_ID;
	static const std::string USER_ID;
	static const std::string PASSWD;
};

class Message{
public:
	enum class MessageType : uint8_t{
		Request,
		Response
	};

    enum class MessageEncoding : int 
    {
      NONE_ENC  = 0, // no encoding 
      JSON_ENC  = 1, // message data uses JSON for encoding
      ASCII_ENC = 2, // message data uses ASCII for encocing
      BYTES_ENC = 3, // message data raw bytes
      ERR_ENC   = -1 // some error happened 
    }; // enum MessageEncoding

	Message() = default;
	Message(uint32_t tranID)
	:tranID_(tranID)
	,msgEnc_(MessageEncoding::NONE_ENC){}

	Message(Message* msg)
	:type_(msg->type_)
	,tranID_(msg->tranID_)
	,data_(std::move(msg->data_))
	,msgEnc_(msg->msgEnc_){}

	Message(Message&& msg)
	:type_(std::move(msg.type_))
	,tranID_(std::move(msg.tranID_))
	,data_(std::move(msg.data_))
	,msgEnc_(std::move(msg.msgEnc_)){}

    Message& operator=(Message&& other)
    {
      if(this != &other)
      {
	    type_   = other.type_;
        tranID_ = other.tranID_;
        data_   = std::move(other.data_);
	    msgEnc_ = other.msgEnc_;
      }

      return *this;
    }



	MessageType                   type_;
	uint32_t                      tranID_;
    std::unique_ptr<folly::IOBuf> data_;    // message data to send  
    MessageEncoding               msgEnc_;  // data type encoding
};

class Request : public Message{
public:
	/*
	enum class OpType : uint8_t{
		READ   = 0,
		WRITE  = 1,
		DELETE = 2,
        COMMIT = 3,
		AUTH   = 4
	};
*/

	Request() = delete;

	/**
	 * If the `opType` is Request::CmmonCode::IOOpCode::AUTH,
	 * Sender will not check the `objectPath`
	 * according to the protocol.
	 * In that case, please set `objectPath` as
	 * arbitary value.
	 */
	Request(uint32_t tranID, const std::string& userID,
		const std::string& password,
		const std::string& objectPath,
		CommonCode::IOOpCode opType)
	:Message(tranID),userID_(userID),password_(password)
	,objectPath_(objectPath),opType_(opType){}

	Request(Request* req)
	:Message(req),userID_(req->userID_),password_(req->password_)
	,objectPath_(req->objectPath_),opType_(req->opType_){}

	Request(Request&& req)
	: Message(std::move(req)),
      userID_(std::move(req.userID_)),
      password_(std::move(req.password_)),
      objectPath_(std::move(req.objectPath_)),
      opType_(std::move(req.opType_))
      {}

    
    Request& operator=(Request&& req)
    {
      if(this != &req)
      {
        // copy Message part first
        Message::operator=(std::move(req));

        // copy specific fields
	    userID_     = std::move(req.userID_);
	    password_   = std::move(req.password_);
        objectPath_ = std::move(req.objectPath_);
	    opType_     = req.opType_;
      }

      return *this;
	}

	std::string 			userID_;  // user id or user name
	std::string 			password_; // secret key or password
	std::string 			objectPath_;
	CommonCode::IOOpCode    opType_;

};

class Response : public Message{
public:
	/*
	enum class Status : uint8_t{
		SUCCESS      = 0, // successful request
		INTERNAL_ERR = 1  // system internal error
	};
*/
	Response() = default;
	Response(uint32_t tranID, CommonCode::IOStatus stateCode)
	:Message(tranID),stateCode_(stateCode){}

	Response(Response* res)
	:Message(res),stateCode_(res->stateCode_){}

	Response(Response&& res)
	: Message(std::move(res)),
      stateCode_(std::move(res.stateCode_))
	  {}


    Response& operator=(Response&& res)
    {

      if(this != &res)
      {
        // copy Message part first
        Message::operator=(std::move(res));
      

        // copy specific fields
	    stateCode_ = res.stateCode_;
      }

      return *this;
	}


	CommonCode::IOStatus stateCode_;

};

/**
* Interface that store and retrive response.
*/
class ReceivePool{
public:
  explicit ReceivePool() = default;
  ~ReceivePool(){
  	boost::mutex::scoped_lock lock(mutex_);
    pool_.clear();
  }

  void insert(std::unique_ptr<Response> res){
    boost::mutex::scoped_lock lock(mutex_);
    pool_.push_back(std::move(res));
  }

  std::vector<std::unique_ptr<Response>> poll(){
    boost::mutex::scoped_lock lock(mutex_);
    DLOG(INFO) << "poll the message";
    std::vector<std::unique_ptr<Response>> res(std::move(pool_));
    pool_.clear();
    
    return res;
  }

private:
  std::vector<std::unique_ptr<Response>> pool_;
  boost::mutex mutex_;
}; // class ReceivePool


} // namespace
