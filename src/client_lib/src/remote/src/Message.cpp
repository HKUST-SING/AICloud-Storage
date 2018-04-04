
/**
 * External lib
 */
#include <folly/dynamic.h>
#include <folly/DynamicConverter.h>

/**
 * Internal lib
 */
#include "remote/Message.h"

namespace folly {
	/**
	 * Customized converter
	 * Convert folly::dynamic to our own structs
	 */
    template <> struct DynamicConverter<
    		singaistorageipc::OpPermissionResponse::MetaData> {
      static singaistorageipc::OpPermissionResponse::MetaData 
      convert(const dynamic& d) {
        uint64_t a = convertTo<uint64_t>(d["GLOBALOBJECTID"]);
        uint64_t b = convertTo<uint64_t>(d["TOTALOBJECTSIZE"]);
        uint32_t c = convertTo<uint32_t>(d["NUMBEROFSHARDS"]);
        return singaistorageipc::OpPermissionResponse::MetaData(a, b, c);
      }
    };

    template <> struct DynamicConverter<
    		singaistorageipc::OpPermissionResponse::RadosObject>{
	  static singaistorageipc::OpPermissionResponse::RadosObject 
	  convert(const dynamic& d) {
        std::string a = convertTo<std::string>(d["CEPHOBJECTID"]);
    	uint32_t b = convertTo<uint32_t>(d["INDEX"]);
        uint64_t c = convertTo<uint64_t>(d["SIZE"]);
        return singaistorageipc::OpPermissionResponse::RadosObject(a, b, c);
      }
	};
}

namespace singaistorageipc{

const std::string MessageField::TRANSACTION_ID = "transaction ID";
const std::string MessageField::USER_ID = "user ID";
const std::string MessageField::PASSWD = "password";
const std::string MessageField::PATH = "object path";
const std::string MessageField::OP_TYPE = "op type";
const std::string MessageField::COMMIT = "commit";	
const std::string MessageField::MessageField::OBJ_SIZE = "object size";
const std::string MessageField::STATE_CODE = "state code";
const std::string MessageField::METADATA = "metadata";
const std::string MessageField::MessageField::RADOSOBJECTS = "rados objects";
const std::string MessageField::FLAG = "flag";

folly::dynamic
Message::encode(){
	folly::dynamic map = folly::dynamic::object(MessageField::TRANSACTION_ID,
							tranID_)(MessageField::USER_ID,userID_);
	return std::move(map);
}

folly::dynamic
Request::encode() {
	folly::dynamic map = Message::encode();
	map.insert(MessageField::PASSWD,password_);
	map.insert(MessageField::PATH,objectPath_);

	return std::move(map);
}

folly::dynamic
AuthenticationRequest::encode() {
	return Request::encode();
}

folly::dynamic
OpPermissionRequest::encode() {
	folly::dynamic map = Request::encode();
	map.insert(MessageField::OP_TYPE,static_cast<uint8_t>(opType_));
	map.insert(MessageField::COMMIT,isCommit_);
	map.insert(MessageField::OBJ_SIZE,objectSize_);

	return std::move(map);
}

void
Message::decode(folly::dynamic map){
	auto kv = map.find(MessageField::TRANSACTION_ID);
	if(kv != map.items().end()){
		tranID_ = folly::convertTo<uint32_t>(kv->second);
	}

	kv = map.find(MessageField::USER_ID);
	if(kv != map.items().end()){
		userID_ = kv->second.asString();
	}
}

void
Response::decode(folly::dynamic map) {
	Message::decode(map);
	auto kv = map.find(MessageField::STATE_CODE);
	if(kv != map.items().end()){
		stateCode_ = static_cast<Response::StateCode>(
						folly::convertTo<uint8_t>(kv->second));
	}
}

void
AuthenticationResponse::decode(folly::dynamic map) {
	Response::decode(map);
}

void
OpPermissionResponse::decode(folly::dynamic map) {
	Response::decode(map);
	auto kv = map.find(MessageField::METADATA);
	if(kv != map.items().end()){
		metaData_ = folly::convertTo<MetaData>(kv->second);
	}

	kv = map.find(MessageField::RADOSOBJECTS);
	if(kv != map.items().end()){
		radosObjects_ = std::move(
							folly::convertTo<std::vector<RadosObject>>(
								kv->second));
	}

	kv = map.find(MessageField::FLAG);
	if(kv != map.items().end()){
		flag_ = kv->second.asBool();
	}
}

} // namespace
