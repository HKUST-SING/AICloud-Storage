/*
 * This file defines the message structure.
 */
#pragma once

/**
 * Standard lib
 */
#include <vector>
#include <map>

/**
 * External lib
 */
#include <folly/dynamic.h>

namespace singaistorageipc{

class MessageField{
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
		AuthenticationRequest,
		OpPermissionRequest,
		AuthenticationResponse,
		OpPermissionResponse
	};

	Message() = default;
	Message(uint32_t tranID, const std::string& userID)
	:tranID_(tranID),userID_(userID){}

	MessageType type_;
	uint32_t tranID_;
	std::string userID_; 	   // user id or user name

	/**
	 * folly::dynamic is a map from dynamic
	 * to dynamic
	 * this method usually is called by the sender
	 */
	virtual folly::dynamic encode();

	/**
	 * this method usually is called by the receiver
	 */
	virtual void decode(folly::dynamic);
};

class Request : public Message{
public:
	Request() = delete;
	Request(uint32_t tranID, const std::string& userID,
		const std::string& password,
		const std::string& objectPath)
	:Message(tranID,userID),password_(password),objectPath_(objectPath){}

	std::string password_; // secret key or password
	std::string objectPath_;

	virtual folly::dynamic encode() override;
	void decode(folly::dynamic m) override{Message::decode(m);};
};

class AuthenticationRequest : public Request{
public:
	AuthenticationRequest() = delete;
	AuthenticationRequest(uint32_t tranID, const std::string& userID,
		const std::string& password)
	:Request(tranID,userID,password,""){
		type_ = Message::MessageType::AuthenticationRequest;
	}

	virtual folly::dynamic encode() override;
};

class OpPermissionRequest : public Request{
public:
	enum class OpType : uint8_t{
		READ   = 0,
		WRITE  = 1,
		DELETE = 2
	};

	OpPermissionRequest() = delete;
	OpPermissionRequest(uint32_t tranID, const std::string& userID,
		const std::string& password, const std::string& objectPath,
		OpType opType, bool isCommit = false)
	:Request(tranID,userID,password,objectPath)
	,opType_(opType),isCommit_(isCommit)
	,objectSize_(0){
		type_ = Message::MessageType::OpPermissionRequest;
	}

	OpType opType_;
	bool isCommit_;
	uint64_t objectSize_; // used for write request case
						  // need to be set explicited

	virtual folly::dynamic encode() override;
};

class Response : public Message{
public:
	enum class StateCode : uint8_t{
		SUCCESS      = 0, // successful request
		USER_ERR     = 1, // no such user found or
						  // user cannot perform operation
		PASSWD_ERR   = 2, // wrong user password
		PATH_ERR	 = 3, // no such path exists
		QUOTA_ERR	 = 4, // user has exceeded data quota
		LOCK_ERR	 = 5, // cannot lock the object
						  // maybe some one is using this object
		INTERNAL_ERR = 6  // system internal error
	};

	Response() = default;
	Response(uint32_t tranID, const std::string& userID, StateCode stateCode)
	:Message(tranID,userID),stateCode_(stateCode){}

	StateCode stateCode_;

	folly::dynamic encode() override{return Message::encode();};
	virtual void decode(folly::dynamic) override;
};

class AuthenticationResponse : public Response{
public:
	AuthenticationResponse(){
		type_ = Message::MessageType::AuthenticationResponse;
	}
	AuthenticationResponse(uint32_t tranID, const std::string& userID,
		StateCode stateCode)
	:Response(tranID,userID,stateCode){
		type_ = Message::MessageType::AuthenticationResponse;
	}

	virtual void decode(folly::dynamic) override;
};

class OpPermissionResponse : public Response{
public:
	typedef struct MetaData{
		uint64_t globalObjectID_;
		uint64_t totalObjectSize_;
		uint32_t numberOfShards_;
		
		MetaData() = default;
		MetaData(uint64_t a, uint64_t b, uint32_t c)
		:globalObjectID_(a),totalObjectSize_(b),numberOfShards_(c){}
	}MetaData;

	typedef struct RadosObject{
		std::string cephObjectID_;
		uint32_t index_;
		uint64_t size_;

		explicit RadosObject(const std::string& a, uint32_t b, uint64_t c)
		:cephObjectID_(a),index_(b),size_(c){}
	}RadosObject;

	OpPermissionResponse(){
		type_ = Message::MessageType::OpPermissionResponse;
	}
	OpPermissionResponse(uint32_t tranID, const std::string& userID,
		StateCode stateCode,
		bool flag, MetaData metaData = {0,0,0},
		std::vector<RadosObject> radosObjects = {})
	:Response(tranID,userID,stateCode)
	,flag_(flag),metaData_(metaData)
	,radosObjects_(std::move(radosObjects)){
		type_ = Message::MessageType::OpPermissionResponse;
	}

	MetaData metaData_;
	std::vector<RadosObject> radosObjects_;
	bool flag_; // both indicate whether the operation
				// need to be pended or the pending
				// operation can be continued

	virtual void decode(folly::dynamic) override;
};
} // namespace