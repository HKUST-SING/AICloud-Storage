// C++ std lib
#include <exception>

#include "JSONRemoteProtocol.h"


using std::shared_ptr;
using std::unique_ptr;


namespace singaistorageipc
{


JSONRemoteProtocol::JSONRemoteProtocol()
{}


bool
JSONRemoteProtocol::doInitialize()
{

  return true;
}


void 
JSONRemoteProtocol::doClose()
{}


shared_ptr<RemoteProtocol::ProtocolHandler>
JSONRemoteProtocol::handleMessage(shared_ptr<IOResponse> ioRes,
                                  const Task& task,
                                  CephContext& cephCtx)
{
  // Pass JSONDecoder to the handler (share one 
  // (among all JSON messages decoder among)

  if(ioRes->msg_->msgEnc_ != Message::MessageEncoding::JSON_ENC || ioRes->opStat_ != CommonCode::IOStatus::STAT_SUCCESS)
  {
    // log this error
    int a = 5;
    
    return std::make_shared<JSONRemoteProtocol::JSONProtocolVoidHandler>(cephCtx, ioRes->opType_);
  }


  // No unexpected errors have occurred
  // use switch to pick one out of a few handlers
  switch(ioRes->opType_)
  {
    case CommonCode::IOOpCode::OP_AUTH:
    { // handle authentication

      auto handler = std::make_shared<JSONRemoteProtocol::JSONProtocolAuthHandler>(cephCtx);

      handler->handleJSONMessage(jsonDecoder_, ioRes, task);

      // return the handler
      return handler;

    }

    case CommonCode::IOOpCode::OP_READ:
    {
      auto handler = std::make_shared<JSONRemoteProtocol::JSONProtocolReadHandler>(cephCtx);

      handler->handleJSONMessage(jsonDecoder_, ioRes, task);

      // return the handler
      return handler;
    }

    case CommonCode::IOOpCode::OP_WRITE:
    {
      auto handler = std::make_shared<JSONRemoteProtocol::JSONProtocolWriteHandler>(cephCtx);

      handler->handleJSONMessage(jsonDecoder_, ioRes, task);
     
      //return the handler
      return handler;
    }

    case CommonCode::IOOpCode::OP_DELETE:
    {
      auto handler = std::make_shared<JSONRemoteProtocol::JSONProtocolDeleteHandler>(cephCtx);

      handler->handleJSONMessage(jsonDecoder_, ioRes, task);

      // return the handler
      return handler;
    }
  

    default:
    {
      int a= 5; // log this case
      return std::make_shared<JSONRemoteProtocol::JSONProtocolVoidHandler>(cephCtx, ioRes->opType_);

    }

  }// switch

}



JSONRemoteProtocol::JSONProtocolVoidHandler::JSONProtocolVoidHandler(CephContext& cephCont, const CommonCode::IOOpCode opCode)
: RemoteProtocol::ProtocolHandler(cephCont, opCode, false)
{
  RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
}


JSONRemoteProtocol::JSONProtocolReadHandler::JSONProtocolReadHandler(CephContext& cephCont)
: RemoteProtocol::ProtocolHandler(cephCont,
                                  CommonCode::IOOpCode::OP_READ,
                                  true)

{
 
}


JSONRemoteProtocol::JSONProtocolReadHandler::~JSONProtocolReadHandler()
{}



void
JSONRemoteProtocol::JSONProtocolReadHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{}


uint64_t
JSONRemoteProtocol::JSONProtocolReadHandler::readData(
                              const uint64_t readBytes,
                              void*          userCtx)
{

  return 0;
}


uint64_t
JSONRemoteProtocol::JSONProtocolReadHandler::getTotalDataSize() const
{

  return 0;
}


JSONRemoteProtocol::JSONProtocolWriteHandler::JSONProtocolWriteHandler(
                  CephContext& cephCont)


: RemoteProtocol::ProtocolHandler(cephCont,  
                                  CommonCode::IOOpCode::OP_WRITE,
                                  true),
  successRes_{nullptr}
{
 
}


JSONRemoteProtocol::JSONProtocolWriteHandler::~JSONProtocolWriteHandler()
{
  successRes_.reset(nullptr);
}



void
JSONRemoteProtocol::JSONProtocolWriteHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{}




uint64_t
JSONRemoteProtocol::JSONProtocolWriteHandler::writeData(
                              const char*    rawData,
                              const uint64_t writeBytes,
                              void*          userCtx)
{

  return 0;
}


unique_ptr<IOResult>
JSONRemoteProtocol::JSONProtocolWriteHandler::getCephSuccessResult()
{
  // Make sure that after swap the remaining pointer is just nullptr
  unique_ptr<IOResult> tmpAns(std::move(successRes_));

  // reset the value of unique
  successRes_.reset(nullptr);
  successRes_.release();

  return std::move(tmpAns);
}



JSONRemoteProtocol::JSONProtocolAuthHandler::JSONProtocolAuthHandler(CephContext& cephCon)
: RemoteProtocol::ProtocolHandler(cephCon,
                   CommonCode::IOOpCode::OP_AUTH,
                    false)
{
}


JSONRemoteProtocol::JSONProtocolAuthHandler::~JSONProtocolAuthHandler()
{

  // clear metadata
  metadata_.clear();

}


void
JSONRemoteProtocol::JSONProtocolAuthHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{

  // try to decode the message and find the required fields
  bool errorFound = !(decoder.decode(ioRes->msg_->data_->data()));
  
  if(errorFound)
  {
    // set the internal error and return
    int a = 5; // log the case
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
   
    return;

  }

  // get a reference to the result object
  auto& jsonRes = decoder.getResult();  

  JSONResult::JSONIter resObj;

  // try to find the proper message fields
  try
  {
    resObj = jsonRes.getObject("Result");
  
  } catch(const std::exception& exp)
  {
    int a = 5; // log the exception
    errorFound = true;
  }


  //check if error has occured
  if(errorFound)
  {
   RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

   return; 
 
  }

  // try to look for an error
  try
  {
    auto accValue = resObj->getValue<std::string>("Account");
    
    //foudn account
    errorFound = false;
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::STAT_SUCCESS;

    // append the account to the metadata map
    metadata_["Username"] = std::move(accValue);
    


  }catch(const std::exception& exp)
  {
    int a = 5; // log the exception
    errorFound = true;
  }

  if(errorFound)
  { // try to find the errror type
    
    try
    {
      //try to retrieve the error type
      auto errType = resObj->getValue<uint64_t>("Error_Type");
      
      // successfully retrieved the error type
      RemoteProtocol::ProtocolHandler::ioStat_ = static_cast<CommonCode::IOStatus>(errType);


    } catch(const std::exception& exp)
      {
        // need to log since there is a problem with the 
        // protocol
        int a = 0; // log the logical error
        RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
   
      }   

  }// if

  // all cases have been handled

}



const std::string&
JSONRemoteProtocol::JSONProtocolAuthHandler::getValue(const std::string& key) const
{

  // look for key

  auto resItr = metadata_.find(key);

  if(resItr == metadata_.end())
  {
    // retturn not found
    return RemoteProtocol::ProtocolHandler::empty_value;
  }
  else
  {
    return resItr->second;
  }

}


const std::string&
JSONRemoteProtocol::JSONProtocolAuthHandler::getValue(const char* key) const
{
  return getValue(std::string(key));
}


JSONRemoteProtocol::JSONProtocolDeleteHandler::JSONProtocolDeleteHandler(CephContext& cephCont)
: RemoteProtocol::ProtocolHandler(cephCont, 
                                  CommonCode::IOOpCode::OP_DELETE,
                                  false)
{}

JSONRemoteProtocol::JSONProtocolDeleteHandler::~JSONProtocolDeleteHandler()
{}


void
JSONRemoteProtocol::JSONProtocolDeleteHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{
  // very simple
  // just try to decode
  const bool success = decoder.decode(ioRes->msg_->data_->data());

  // make sure it's JSON
  if(success)
  {
  RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::STAT_SUCCESS;
  }  
  else
  {
    // not a JSON file
    int a = 0; // log this as an error 

    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

  }


}


} // namesapce singaitstoraipc
