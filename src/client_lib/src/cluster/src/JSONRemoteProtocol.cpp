// C++ std lib
#include <exception>
#include <algorithm>
#include <cassert>


#include "JSONRemoteProtocol.h"


using std::shared_ptr;
using std::unique_ptr;
using std::string;


// for ensuring proper conversion between size_t and uint64_t
static constexpr const size_t MAX_SIZE_T = std::numeric_limits<size_t>::max();

static constexpr const uint64_t SIZE_T_U64 = static_cast<const uint64_t>(std::numeric_limits<size_t>::max());


static constexpr const uint64_t MAX_U64 = std::numeric_limits<uint64_t>::max();



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
                                  true),
  totalSize_(0),
  readObject_(0)
{
 
}


JSONRemoteProtocol::JSONProtocolReadHandler::~JSONProtocolReadHandler()
{

  // clear objects
  radObjs_.clear();

}



void
JSONRemoteProtocol::JSONProtocolReadHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{}



bool
JSONRemoteProtocol::JSONProtocolReadHandler::doneReading() const
{

  return (totalSize_ == readObject_);

}


uint64_t
JSONRemoteProtocol::JSONProtocolReadHandler::readData(
                              const uint64_t readBytes,
                              void*          userCtx)
{

  // try to find an existing Rados object which contains
  // info
  auto radosIter = std::find_if(std::begin(radObjs_), std::end(radObjs_),
                   [this](const RadosObjRead& radObj) -> bool
                   { return (this->readObject_ >= radObj.global_ && this->readObject_ < (radObj.global_ + radObj.size_));});


  // check if an object at the offset exists
  if (radosIter == std::end(radObjs_))
  {
    int a = 5; // log this as an error
    
    return 0;
  }


  //found a valid object
  // cannot read more than left of the object
  const uint64_t capRadosSize = ((radosIter->size_ - radosIter->offset_) < readBytes) ? (radosIter->size_ - radosIter->offset_) : readBytes;

  // cap possible reading size
  const size_t tryRead = (SIZE_T_U64 < capRadosSize) ? MAX_SIZE_T : static_cast<size_t>(capRadosSize);

  
  // try to issue a read operation to the librados API   
  const size_t willRead = cephCtx_.readObject(
                              radosIter->objID,
                              radosIter->poolName,
                              tryRead, // try to read than many bytes
                              radosIter->offset_,
                              userCtx);


  if(!willRead)
  {
    //log this since there is an error
    int a = 0;
    
    return 0;
  }

  // update the internal values and return 
  // the number of bytes to be read
  const uint64_t acceptedBytes = static_cast<const uint64_t>(willRead);

  // update the offset
  radosIter->offset_ += acceptedBytes; // 

  // update the global offset (storage system object offset)
  // overflow shall never happen
  readObject_ = ((readObject_ + acceptedBytes) < acceptedBytes) ? MAX_U64 :  (readObject_ + acceptedBytes);


  // avoid any wrong offsets (log them if they ever occur)
  assert(readObject_ <= totalSize_);


  // eventually return the number of bytes accepted
  // by the lower layer

  return acceptedBytes;
}

bool
JSONRemoteProtocol::JSONProtocolReadHandler::resetDataOffset(const uint64_t offset)
{

  if(offset >= totalSize_)
  {
    return false;
  }

 
  readObject_ = offset; // reset the current data offset


  return true;

}

uint64_t
JSONRemoteProtocol::JSONProtocolReadHandler::getTotalDataSize() const
{

  return totalSize_;
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
