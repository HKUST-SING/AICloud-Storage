// C++ std lib
#include <exception>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <cctype>



// Google libs
#include <glog/logging.h>

#include "JSONRemoteProtocol.h"


using std::shared_ptr;
using std::unique_ptr;
using std::string;
using JSONIter = singaistorageipc::JSONResult::JSONIter;


// for ensuring proper conversion between size_t and uint64_t
static constexpr const size_t MAX_SIZE_T = std::numeric_limits<size_t>::max();

static constexpr const uint64_t SIZE_T_U64 = static_cast<const uint64_t>(std::numeric_limits<size_t>::max());


static constexpr const uint64_t MAX_U64 = std::numeric_limits<uint64_t>::max();


// This function is similar to the one found in rgw_rados.h (Ceph rgw)
static inline void prependBucketMarker(const std::string& marker,
                                       const std::string& origID,
                                       std::string& oid)
{

  if(marker.empty() || origID.empty())
  {
   oid = origID;
  }
  else
  {
    // prepend bucket marker (tail objects follow such pattern)
    oid = marker,
    oid.append("_");
    oid.append(origID);
  }

}


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
    LOG(WARNING) << "JSONRemoteProtocol::handleMessage: "
                 << "!= MessageEncoding::JSON_ENC || "
                 << "!=STAT_SUCCESS";
    
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
   
   case  CommonCode::IOOpCode::OP_COMMIT:
    {
      auto handler = std::make_shared<JSONRemoteProtocol::JSONProtocolStatusHandler>(cephCtx);

      handler->handleJSONMessage(jsonDecoder_, ioRes, task);
     
      // return the handler
      return handler;
    }
  

    default:
    {
      LOG(WARNING) << "JSONRemoteProtocol::handleMessage: "
                   << "unrecognised IOOpCode";
      return std::make_shared<JSONRemoteProtocol::JSONProtocolVoidHandler>(cephCtx, ioRes->opType_);

    }

  }// switch

}


JSONRemoteProtocol::JSONProtocolStatusHandler::JSONProtocolStatusHandler(CephContext& cephCont)
: RemoteProtocol::ProtocolHandler(cephCont, CommonCode::IOOpCode::OP_COMMIT,
  false)
{}

void
JSONRemoteProtocol::JSONProtocolStatusHandler::handleJSONMessage(
JSONDecoder<const uint8_t*>& decoder, shared_ptr<IOResponse> ioRes,
const Task& task)
{
  // try to decode the message and find the required fields
  bool errorFound = !(decoder.decode(ioRes->msg_->data_->data()));
  
  if(errorFound)
  {
    // set the internal error and return
    LOG(WARNING) << "JSONProtocolStatusHandler::handleJSONMessage: "
                 << "deocoder.decode failed";
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
   
    return;

  }

  // get a reference to the result object
  auto& jsonRes = decoder.getResult();  

  JSONIter resObj;

  // try to find the proper message fields
  try
  {
    resObj = jsonRes.getObject("Result");
  
  } catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolstatusHandler::handleJSONMessage: "
                 << "cannot find 'Result' field";
    errorFound = true;
  }


  //check if error has occured
  if(errorFound)
  {
   RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

   return; 
 
  }

  // try to decode the status message
  try
  {
    // get status type
    auto errType = resObj->getValue<uint64_t>("Error_Type");
    // cast the error type (it also return status SUCCESS)
    RemoteProtocol::ProtocolHandler::ioStat_ = static_cast<CommonCode::IOStatus>(errType);
    // done

  }catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolStatusHandler::handleJSONMessage: "
                 << "cannot find 'Erro_Tyoe' field";
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
  }
 

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
{

  // try to decode the message and find the required fields
  bool errorFound = !(decoder.decode(ioRes->msg_->data_->data()));
  
  if(errorFound)
  {
    // set the internal error and return
    LOG(WARNING) << "JSONProtocolReadHandler::handleJSONMessage: "
                 << "decoder.decode failed.";
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
   
    return;

  }

  // get a reference to the result object
  auto& jsonRes = decoder.getResult();  

  JSONIter resObj;

  // try to find the proper message fields
  try
  {
    resObj = jsonRes.getObject("Result");
  
  } catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolReadHandler::handleJSONMessage: "
                 << "cannot find 'Result' field";
    errorFound = true;
  }


  //check if error has occured
  if(errorFound)
  {
   RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

   return; 
 
  }

  JSONIter radosObjIter;
  // try to look for the object size and Rados objects
  try
  {
    auto objSize  = resObj->getValue<uint64_t>("Object_Size");
    
    if(objSize == 0)
    {
      RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
      errorFound = true;
      LOG(WARNING) << "JSONProtocolReadHandler::handleJSONMessage: "
                   << "'Object_Size' == 0";
  
    }
    else
    {
      // try to retrieve the objects 
      // (build internal representation of a storage Object)
      radosObjIter = resObj->getObject("Rados_Objs");
      
      // retrieved the objects
      errorFound = false;
      totalSize_ = objSize;
     
    }
  

  }catch(const std::exception& exp)
  {
    DLOG(INFO) << "JSONProtocolReadHandler::handleJSONMessage: "
               << "cannot find either 'Obj_Size' or 'Rados_Objs'.";
    errorFound = true;
  }


  if(errorFound)
  { // try to find the error type
    
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
        LOG(WARNING) << "JSONProtocolReadHandler::handleJSONMessage: "
                     << "can find neither 'Obj_Size'/'Rados_Objs' "
                     << "nor 'Error_Type'";
        RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
   
      }   

  }// if
  else // no error has been found (found 'Obj_Size' and 'Rados_Objs')
  {
    auto objRes = processObjects(radosObjIter);
    
    if(objRes == CommonCode::IOStatus::STAT_SUCCESS)
    { // objects have been read

      RemoteProtocol::ProtocolHandler::ioStat_ = objRes;

    }    
    else
    { // some problems with the objects
      totalSize_ = 0; // reset object size to 0
      RemoteProtocol::ProtocolHandler::ioStat_ = objRes;
    }

  }//else
  

}


CommonCode::IOStatus
JSONRemoteProtocol::JSONProtocolReadHandler::processObjects(const JSONResult::JSONIter& objIter)
{

  // must make sure that it's a loop object
  if(!objIter || objIter.end())
  {
    LOG(ERROR) << "JSONProtocolReadHandler::processObjects: "
               << "!objIter || objIter.end()";
    return CommonCode::IOStatus::ERR_INTERNAL;
  
  }

  // prepare to return
  auto objsRet = CommonCode::IOStatus::STAT_SUCCESS;
  // Iterate over objects and create a vector of objects
  uint64_t globalOffset = 0; // for efficient search

  try
  {
    for(auto tmpIter = objIter.begin(); !tmpIter.end(); ++tmpIter)
    {
      auto radosPool = tmpIter->getValue<std::string>("pool");
      auto radosID   = tmpIter->getValue<std::string>("oid");
      auto radosSize = tmpIter->getValue<uint64_t>("size");
      
      // successfully retrieved a Rados object
      radObjs_.push_back(std::move(
        JSONProtocolReadHandler::RadosObjRead(std::move(radosPool),
                                              std::move(radosID),
                                              radosSize,
                                              0,
                                              globalOffset)));


       // update the global offset value for future processing
       globalOffset += radosSize;
    }

  } catch(const std::exception& exp)
  {
    objsRet = CommonCode::IOStatus::ERR_INTERNAL;
    radObjs_.clear(); // clear the read objects
  }

  // make sure that the read object size
  // is equal to the per Rados object size
  assert(totalSize_ == globalOffset);


  return objsRet; // return the status code
}



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

  // first check if reading completed
  if(doneReading() || RemoteProtocol::ProtocolHandler::ioStat_ != CommonCode::IOStatus::STAT_SUCCESS || readBytes == 0)
  {
    return 0; /* nothing to read*/
  }


  // try to find an existing Rados object which contains
  // info
  auto radosIter = std::find_if(std::begin(radObjs_), std::end(radObjs_),
                   [this](const RadosObjRead& radObj) -> bool
                   { return (this->readObject_ >= radObj.global_ && this->readObject_ < (radObj.global_ + radObj.size_));});


  // check if an object at the offset exists
  if (radosIter == std::end(radObjs_))
  {
    LOG(WARNING) << "JSONProtocolReadHandler::readData: "
                 << "radositer == std::end(radObjs_)";
    
    return 0;
  }


  //found a valid object
  // cannot read more than left of the object
  const uint64_t capRadosSize = ((radosIter->size_ - radosIter->offset_) < readBytes) ? (radosIter->size_ - radosIter->offset_) : readBytes;

  // cap possible reading size
  const size_t tryRead = (SIZE_T_U64 < capRadosSize) ? MAX_SIZE_T : static_cast<size_t>(capRadosSize);


  if(!tryRead)
  {
    // soemthing wrong (retry later)
    return 0;
  }

  
  // try to issue a read operation to the librados API   
  const size_t willRead = cephCtx_.readObject(
                              radosIter->objID,
                              radosIter->poolName,
                              tryRead, // try to read that many bytes
                              radosIter->offset_,
                              userCtx);


  if(!willRead)
  {
    //log this since there is an error
    LOG(WARNING) << "JSONProtocolReadHandler::readData: "
                 << "willRead == 0";
    
    return 0;
  }

  // update the internal values and return 
  // the number of bytes to be read
  const uint64_t acceptedBytes = static_cast<const uint64_t>(willRead);

  // update the offset
  radosIter->offset_ += acceptedBytes;  

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

  if(offset > totalSize_)
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



uint64_t
JSONRemoteProtocol::JSONProtocolReadHandler::getDataOffset() const
{

  return readObject_;
}


JSONRemoteProtocol::JSONProtocolWriteHandler::JSONProtocolWriteHandler(
                  CephContext& cephCont)


: RemoteProtocol::ProtocolHandler(cephCont,  
                                  CommonCode::IOOpCode::OP_WRITE,
                                  true),
  successRes_{nullptr},
  avSuccess_(false),
  writeOffset_(0),
  writeSize_(0)
{
 
}


JSONRemoteProtocol::JSONProtocolWriteHandler::~JSONProtocolWriteHandler()
{
  successRes_.reset(nullptr);
  radosWrites_.clear();
}



uint64_t
JSONRemoteProtocol::JSONProtocolWriteHandler::getDataOffset() const
{

  return writeOffset_;
}

bool
JSONRemoteProtocol::JSONProtocolWriteHandler::resetDataOffset(const uint64_t offset)
{

  writeOffset_ = offset;

  return true;


}

void
JSONRemoteProtocol::JSONProtocolWriteHandler::handleJSONMessage(
                            JSONDecoder<const uint8_t*>& decoder, 
                            std::shared_ptr<IOResponse> ioRes, 
                            const Task& task)
{

  // go over different stages of decoding the message and
  // check for status
  bool errorFound = !(decoder.decode(ioRes->msg_->data_->data()));
  
  if(errorFound)
  {
    // set the internal error and return
    LOG(WARNING) << "JSONProtocolWriteHandler::handleJSONMessage: "
                 << "decoder.decode failed.";
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    return;
  }
  

  // get a reference to the result object
  auto& jsonRes = decoder.getResult();

  JSONIter resObj;

  // try to find the 'Result' field which encapsulates
  // all results (successes and errors)
  try
  {
    resObj = jsonRes.getObject("Result");
  }catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolWriteHandler::handleJSONMessage: "
                 << "cannot find 'Result' field";
    errorFound = true; 
  }


  // check the result of the previous stage ('Result')
  if(errorFound)
  {
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    return; // no need to process further

  }

  // get a reference to 'Rados_Objs'
  JSONIter radosIter;

  try
  {
    radosIter = resObj->getObject("Rados_Objs");

    // check if 'Data_Manifest' exists
    auto manifestIter = resObj->getObject("Data_Manifest");

  }catch(const std::exception& exp)
  {
    DLOG(INFO) << "JSONProtocolWriteHandler::handleJSONMessage: "
               << "'Rados_Objs' or 'Data_Manifest' does not exist. "
               << "Checking for 'Error_Type'";
    errorFound = true;
  }


  if(errorFound)
  {
    // 'Rados_Objs' does not exist; check for 'Error_Type'
    try
    {
      auto errType = resObj->getValue<uint64_t>("Error_Type");
      
      // successfully retrieved the error type
      RemoteProtocol::ProtocolHandler::ioStat_ = static_cast<CommonCode::IOStatus>(errType); // cast the error type

    }catch(const std::exception& exp)
    {
      // something went wrong
      LOG(WARNING) << "JSONProtocolWriteHandler::handleJSONMessage: "
                   << "can find neither 'Rados_Objs' nor 'Error_Type'.";

      RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;
    }

  }// if errorFound
  else
  { // no errors have been found ('Rados_Objs' exists)
    // process the 'Rados_Objs' field
    RemoteProtocol::ProtocolHandler::ioStat_ = processObjects(radosIter);

    if(RemoteProtocol::ProtocolHandler::ioStat_ == CommonCode::IOStatus::STAT_SUCCESS)
    {
      // prepare the return message
      auto prepRes = prepareResult(ioRes, task);
      
      if(prepRes != CommonCode::IOStatus::STAT_SUCCESS)
      {
        RemoteProtocol::ProtocolHandler::ioStat_ = prepRes;
      }
      else
      {
        avSuccess_ = true; // need to send a response back on success
      }
      
    }// if STAT_SUCCESS

  }// else (errorFound)

}


CommonCode::IOStatus
JSONRemoteProtocol::JSONProtocolWriteHandler::processObjects(const JSONResult::JSONIter& radosObjs)
{
  // make sure it is not empty
  if(!radosObjs || radosObjs.end())
  {
    LOG(WARNING) << "JSONProtocolWriteHandler::processObjects: "
                 << "!radosObjs || radosObjs.end()";
    return CommonCode::IOStatus::ERR_INTERNAL;
  }

  // set return result
  auto objsRet = CommonCode::IOStatus::STAT_SUCCESS;

  // Iterate over objects and create a vector for future
  // writing of objects.

  uint64_t globalOffset = 0;

  try
  {
    for(auto tmpIter = radosObjs.begin(); !tmpIter.end(); ++tmpIter)
    {
      auto radosPool   = tmpIter->getValue<std::string>("pool");
      auto radosID     = tmpIter->getValue<std::string>("oid");
      auto radosSize   = tmpIter->getValue<uint64_t>("size");
      auto radosOffset = tmpIter->getValue<uint64_t>("offset");
      auto radosWrite  = tmpIter->getValue<uint64_t>("new_object");

      // retrieved all needed values
      // process them for future use
      if(radosSize <= radosOffset)
      {
        LOG(ERROR) << "JSONProtocolWriteHandler::processObjects: "
                   << "radosSize <= radosOffset";
        throw std::exception();
      }

      radosWrites_.push_back(std::move(
        JSONProtocolWriteHandler::RadosObjWrite(
          std::move(radosPool), std::move(radosID), 
          (radosSize - radosOffset), radosOffset,
          (radosWrite) ? false : true, globalOffset)));

      // upgrade the global offset
      globalOffset += (radosSize - radosOffset);

    }//for
 
    writeSize_ = globalOffset; // total size to write 

  }catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolWriteHandler::processObjects: "
                 << "exception reading the received JSON message.";
    radosWrites_.clear(); // clear the retrieved objects
    objsRet = CommonCode::IOStatus::ERR_INTERNAL;
  }

  return objsRet;

}



bool
JSONRemoteProtocol::JSONProtocolWriteHandler::needNotifyCephSuccess() const
{
  return avSuccess_;
}


uint64_t
JSONRemoteProtocol::JSONProtocolWriteHandler::getTotalDataSize() const
{
  return writeSize_;
}


bool
JSONRemoteProtocol::JSONProtocolWriteHandler::doneWriting() const
{
  return (writeOffset_ >= writeSize_);
}

uint64_t
JSONRemoteProtocol::JSONProtocolWriteHandler::writeData(
                              const char*    rawData,
                              const uint64_t writeBytes,
                              void*          userCtx)
{
  if(RemoteProtocol::ProtocolHandler::ioStat_ != CommonCode::IOStatus::STAT_SUCCESS || radosWrites_.empty() || !rawData || writeBytes == 0)
  {
    return 0; // don't write if failure has occurred
  }

  // try to find an existing Rados object which contains
  // info
  auto radosIter = std::find_if(std::begin(radosWrites_), 
                                std::end(radosWrites_), 
                                [this](const RadosObjWrite& radObj) -> bool {return (this->writeOffset_ >= radObj.global_ && this->writeOffset_ < radObj.global_);});


  if(radosIter == std::end(radosWrites_))
  {
    LOG(WARNING) << "JSONProtocolWriteHandler::writeData: "
                 << "radosIter == std::end(radosWrites_).";
    return 0;
  }

  // found a valid object
  const uint64_t capRadosSize = (radosIter->avSize_ > writeBytes) ? writeBytes : radosIter->avSize_;

  // cap the possible write size
  const size_t tryWrite = (SIZE_T_U64 < capRadosSize) ? MAX_SIZE_T : static_cast<const size_t>(capRadosSize);
 
  if(!tryWrite)
  {
    return 0; // cannot write data (perhaps retry later)
  }


  const size_t willWrite = cephCtx_.writeObject(
                                    radosIter->objID,
                                    radosIter->poolName,
                                    rawData,
                                    tryWrite,
                                    radosIter->offset_,
                                    radosIter->append_,
                                    userCtx);

  
  if(!willWrite)
  {
    // log this case as an error
    LOG(WARNING) << "JSONProtocolWriteHandler::writeData: "
                 << "willWrite == 0";

    return 0;
  }

  const uint64_t acceptedBytes = static_cast<const uint64_t>(willWrite);

  radosIter->offset_ += acceptedBytes;

 
  // update the global write operation
  writeOffset_ += acceptedBytes;
  
  
  return acceptedBytes;
}


uint64_t
JSONRemoteProtocol::JSONProtocolWriteHandler::writeData(
                              const librados::bufferlist& buffer,
                              void*          userCtx)
{


  if(RemoteProtocol::ProtocolHandler::ioStat_ != CommonCode::IOStatus::STAT_SUCCESS || radosWrites_.empty() || buffer.length() == 0)
  {
    return 0; // don't write if failure has occurred
  }

  // try to find an existing Rados object which contains
  // info
  auto radosIter = std::find_if(std::begin(radosWrites_), 
                                std::end(radosWrites_), 
                                [this](const RadosObjWrite& radObj) -> bool {return (this->writeOffset_ >= radObj.global_ && this->writeOffset_ < radObj.global_);});


  if(radosIter == std::end(radosWrites_))
  {
    LOG(WARNING) << "JSONProtocolWriteHandler::writeData: "
                 << "radosIter == std::end(radosWrites_)";
    return 0;
  }


  // found a valid object

  const uint64_t writeBytes = static_cast<const uint64_t>(buffer.length());
  const uint64_t capRadosSize = (radosIter->avSize_ > writeBytes) ? writeBytes : radosIter->avSize_;

  // cap the possible write size
  const size_t tryWrite = (SIZE_T_U64 < capRadosSize) ? MAX_SIZE_T : static_cast<const size_t>(capRadosSize);
 
  if(!tryWrite)
  {
    return 0; // cannot write data (perhaps retry later)
  }


  const size_t willWrite = cephCtx_.writeObject(
                                    radosIter->objID,
                                    radosIter->poolName,
                                    buffer,
                                    tryWrite,
                                    radosIter->offset_,
                                    radosIter->append_,
                                    userCtx);

  
  if(!willWrite)
  {
    // log this case as an error
    LOG(WARNING) << "JSONProtocolWriteHandler::writeData: "
                 << "willWrite == 0";

    return 0;
  }

  const uint64_t acceptedBytes = static_cast<const uint64_t>(willWrite);

  radosIter->offset_ += acceptedBytes;

 
  // update the global write operation
  writeOffset_ += acceptedBytes;
  
  
  return acceptedBytes;


}
 

CommonCode::IOStatus 
JSONRemoteProtocol::JSONProtocolWriteHandler::prepareResult(shared_ptr<IOResponse> ioRes, const Task& task)
{
  // prepare a response message for a successful IO Operation
  auto tmpUnique = std::make_unique<IOResult>(
    std::move(std::make_unique<Request>(
      0, task.username_, RemoteProtocol::ProtocolHandler::empty_value,
      task.path_, CommonCode::IOOpCode::OP_COMMIT)));

  successRes_.swap(tmpUnique);

  // created a successful message
  
  /// finish filling the required information
  successRes_->msg_->msgEnc_ = Message::MessageEncoding::JSON_ENC;
  successRes_->msg_->data_.swap(ioRes->msg_->data_);

  
  return CommonCode::IOStatus::STAT_SUCCESS;

}

unique_ptr<IOResult>
JSONRemoteProtocol::JSONProtocolWriteHandler::getCephSuccessResult()
{

  if(!avSuccess_)
  {
    return unique_ptr<IOResult>{nullptr};
  }
  else
  {

    // Make sure that after swap the remaining pointer is just nullptr
    unique_ptr<IOResult> tmpAns(std::move(successRes_));

    // reset the value of unique
    successRes_.reset(nullptr);
    successRes_.release();
    avSuccess_ = false; // not available anymore

    return tmpAns;

  } //else
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
    LOG(WARNING) << "JSONProtocolAuthHandler::handleJSONMessage: "
                 << "decoder.decode failed.";
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
    LOG(WARNING) << "JSONProtocolAuthHandler::handleJSONMessage: " 
                 << "cannot find 'Result' field.";
    errorFound = true;
  }


  //check if error has occurred
  if(errorFound)
  {
   RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

   return; 
 
  }

  // try to look for the successful check
  try
  {
    auto accValue = resObj->getValue<std::string>("Account");
    
    //found account
    errorFound = false;
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::STAT_SUCCESS;

    // append the account to the metadata map
    metadata_["Username"] = std::move(accValue);
    


  }catch(const std::exception& exp)
  {
    DLOG(INFO) << "JSONProtocolAuthHandler::handleJSONMessage: " 
               << "'Account' field not found. Trying 'Error_Type'.";
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
        LOG(WARNING) << "JSONProtocolAuthHandler::handleJSONMessage: "
                     << "can find neither 'Account' nor 'Error_Type'.";
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
    // return not found
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
    LOG(WARNING) << "JSONProtocolDeleteHandler::handleJSONMessage: "
                 << "decoder.decode failed."; 

    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    return; // no more processing

  }


  auto& jsonRes = decoder.getResult();
  JSONIter resObj;
  bool failFound  = false;
 
  // try to look for error messages
  try
  {
    resObj = jsonRes.getObject("Result");
  } catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolDeleteHandler::handleJSONMessage: "
                 << "'Result' NOT found.";

    failFound = true;
  }
  
  if(failFound)
  {
    // set status to ERR_INTERNAL
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

    return;

  }
 

  try
  {
    std::string resMsg = resObj->getValue<std::string>("Response_Status");
    RemoteProtocol::ProtocolHandler::ioStat_ = responseStatus(resMsg);  
    
  } catch(const std::exception& exp)
  {
    LOG(WARNING) << "JSONProtocolDeleteHandler::handleJSONMessage: "
                 << "cannot find 'Response_Status'.";

    failFound = true;
  }

   if(failFound)
  {
    // set status to ERR_INTERNAL
    RemoteProtocol::ProtocolHandler::ioStat_ = CommonCode::IOStatus::ERR_INTERNAL;

   }
 

}

CommonCode::IOStatus
JSONRemoteProtocol::JSONProtocolDeleteHandler::responseStatus(
                                  const std::string& resMsg) const
{

  // try to extract the response code from the message
  std::stringstream ss("", std::ios_base::app | std::ios_base::out); 
  
  // go over the result string an look for a number
  for(const auto& val : resMsg)
  {
    if(std::isdigit(val))
    {
      ss << val; // append all digits
    }
  }

  if(ss.str().empty())
  { // no digits (error)
    return CommonCode::IOStatus::ERR_INTERNAL;
  }

  // convert into a value
  auto resCode = std::stoul(ss.str(), nullptr, 10);
  
  // 200 means its a success
  // 400 menas no such path/file
  // other values is an internal error
  
  switch(resCode)
  {
    case 200:
    {
      return CommonCode::IOStatus::STAT_SUCCESS;
    }
    case 400:
    {
      return CommonCode::IOStatus::ERR_PATH;
    }
    default:
      break;

  }//switch
  
  return CommonCode::IOStatus::ERR_INTERNAL;

}

} // namesapce singaitstoraipc
