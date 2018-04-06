/*
 * This file defines the message structure.
 */
#pragma once

/**
 * External lib
 */
#include <folly/io/IOBuf.h>


namespace singaistorageipc{

class RESTHeadField{
public:
	static const std::string TRANSACTION_ID;
	static const std::string USER_ID;
	static const std::string PASSWD;
	static const std::string PATH;
	static const std::string OP_TYPE;
	static const std::string COMMIT;	
	static const std::string OBJ_SIZE;
	static const std::string STATE_CODE;
	static const std::string METADATA;
	static const std::string RADOSOBJECTS;
	static const std::string FLAG;
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
	Message(uint32_t tranID, const std::string& userID)
	:tranID_(tranID),userID_(userID)
	,msgEnc_(MessageEncoding::NONE_ENC){}

	MessageType                   type_;
	uint32_t                      tranID_;
	std::string                   userID_;  // user id or user name
    std::unique_ptr<folly::IOBuf> data_;    // message data to send  
    MessageEncoding               msgEnc_;  // data type encoding
};

class Request : public Message{
public:
	enum class OpType : uint8_t{
		READ   = 0,
		WRITE  = 1,
		DELETE = 2,
        	COMMIT = 3,
		AUTH   = 4
	};


	Request() = delete;
	Request(uint32_t tranID, const std::string& userID,
		const std::string& password,
		const std::string& objectPath,
		OpType opType)
	:Message(tranID,userID),password_(password)
	,objectPath_(objectPath),opType_(opType){}

	std::string password_; // secret key or password
	std::string objectPath_;
	OpType      opType_;

};

class Response : public Message{
public:
	enum class StateCode : uint8_t{
		SUCCESS      = 0, // successful request
		INTERNAL_ERR = 1  // system internal error
	};

	Response() = default;
	Response(uint32_t tranID, const std::string& userID, StateCode stateCode)
	:Message(tranID,userID),stateCode_(stateCode){}

	StateCode stateCode_;

};

} // namespace
