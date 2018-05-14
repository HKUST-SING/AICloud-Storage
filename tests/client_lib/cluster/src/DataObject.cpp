// C++ std lib
#include <cstring>
#include <limits>
#include <stdexcept>



// Google libs
#include <glog/logging.h>

// Project lib
#include "DataObject.h"



static constexpr const unsigned int MAX_RADOS_OP = (std::numeric_limits<unsigned int>::max() >> 1);

static constexpr const unsigned int MAX_UINT_SIZE = std::numeric_limits<unsigned int>::max();

static constexpr const uint64_t MAX_U64_T = std::numeric_limits<uint64_t>::max();

namespace singaistorageipc
{

namespace radosbuffermanagement
{


DataObject::DataObject()
: ptr_(),
  opType_(CommonCode::IOOpCode::OP_NOP)
{
  ptr_.writePtr = nullptr;
  ptr_.readPtr  = nullptr;
}


DataObject::DataObject(DataObject&& other)
{

  switch(other.opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      ptr_.writePtr = other.ptr_.writePtr;
      opType_ = CommonCode::IOOpCode::OP_WRITE;

      break;
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      ptr_.readPtr = other.ptr_.readPtr;
      opType_ = CommonCode::IOOpCode::OP_READ;

      break;
    }
    default:
    {
      ptr_.writePtr = nullptr;
      ptr_.readPtr  = nullptr;
      opType_       = CommonCode::IOOpCode::OP_NOP;
    }
      
  } // switch

  other.ptr_.writePtr  =  nullptr;
  other.ptr_.readPtr   =  nullptr;
  other.opType_        =  CommonCode::IOOpCode::OP_NOP;
}


DataObject::~DataObject()
{
  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      delete ptr_.writePtr;

      break;
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      delete ptr_.readPtr; 

      break;
    }
    default:
    {
      break;
    }
      
  } // switch

  ptr_.writePtr = nullptr;
  ptr_.readPtr  = nullptr;
  opType_       = CommonCode::IOOpCode::OP_NOP;

}

CommonCode::IOStatus
DataObject::getObjectOpStatus() const noexcept
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->getObjectOpStatus();
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->getObjectOpStatus();
    }
  
    default:
    {
      break;
    }

  } //switch


  return CommonCode::IOStatus::ERR_INTERNAL;

}



void
DataObject::setObjectOpStatus(const CommonCode::IOStatus status)
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      ptr_.writePtr->setObjectOpStatus(status);
      break;
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      ptr_.readPtr->setObjectOpStatus(status);
      break;
    }
  
    default:
    {
      break;
    }

  } //switch

}



bool
DataObject::isComplete() const noexcept
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->isComplete();
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->isComplete();
    }
  
    default:
    {
      break;
    }

  } //switch


  return true; // nothing to complete


}


WriteObject*
DataObject::getWriteObject(const bool claim)
{

  if(opType_ == CommonCode::IOOpCode::OP_WRITE)
  {
    WriteObject* tmpPtr = ptr_.writePtr;
    
    if(claim) // pass ownership to the caller
    {
      ptr_.writePtr = nullptr;
      opType_ = CommonCode::IOOpCode::OP_NOP;
    }   

    return tmpPtr;
  }
  else
  {
    return nullptr;
  }

}


bool
DataObject::replaceUserContext(UserCtx&& ctx)
{

   switch(opType_)
   {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->replaceUserContext(std::move(ctx));
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->replaceUserContext(std::move(ctx));
    }
  
    default:
    {
      break;
    }

  } //switch


  return false; // nothing to set

}


bool
DataObject::validUserContext() const
{
  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->validUserContext();
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->validUserContext();
    }
  
    default:
    {
      break;
    }

  } //switch


  return false; // no user context

}


bool
DataObject::setResponse(Task&& response)
{
  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->setResponse(std::move(response));
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->setResponse(std::move(response));
    }
  
    default:
    {
      break;
    }

  } //switch


  return false; // nothing to set

}


const Task&
DataObject::getTask() const
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->getTask();
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->getTask();
    }
  
    default:
    {
      break;
    }

  } //switch


  throw std::logic_error("Empty DataObject."); // no result

}



const DataObject::UserCtx&
DataObject::getUserContext() const
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->getUserContext();
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->getUserContext();
    }
  
    default:
    {
      break;
    }

  } //switch


  throw std::logic_error("Empty DataObject."); // problem

}





Task&
DataObject::getTask(const bool moveVal)
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->getTask(moveVal);
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->getTask(moveVal);
    }
  
    default:
    {
      break;
    }

  } //switch


  throw std::logic_error("Empty DataObject.");

}



DataObject::UserCtx&
DataObject::getUserContext(const bool moveVal)
{

  switch(opType_)
  {
    case CommonCode::IOOpCode::OP_WRITE:
    {
      return ptr_.writePtr->getUserContext(moveVal);
    }
    case CommonCode::IOOpCode::OP_READ:
    {
      return ptr_.readPtr->getUserContext(moveVal);
    }
  
    default:
    {
      break;
    }

  } //switch


  throw std::logic_error("Empty DataObject.");

}




ReadObject*
DataObject::getReadObject(const bool claim)
{
  if(opType_ == CommonCode::IOOpCode::OP_READ)
  {
    ReadObject* tmpPtr = ptr_.readPtr;

    if(claim)
    { // pass the ownership to the caller
      ptr_.readPtr = nullptr;
      opType_ = CommonCode::IOOpCode::OP_NOP;
    }
 
    return tmpPtr;
  }
  else
  {
    return nullptr;
  }

}


void
DataObject::setWriteObject(WriteObject* obj)
{

  // check if non empty
  if(opType_ != CommonCode::IOOpCode::OP_NOP)
  {
    // delete the current object before reseting the new one
    switch(opType_)
    {
      case CommonCode::IOOpCode::OP_WRITE:
      {
        delete ptr_.writePtr;
        ptr_.writePtr = nullptr;
        break;
      }
      case CommonCode::IOOpCode::OP_READ:
      {
        delete ptr_.readPtr;
        ptr_.readPtr = nullptr;
        break;
      }
  
      default:
      {
        LOG(WARNING) << "DataObject::opType_: "
                     << "neither OP_WRITE nor OP_READ"; 
        break;
      }

    } //switch
    
  }//if


  opType_ = CommonCode::IOOpCode::OP_WRITE;
  ptr_.writePtr = obj;

}


void
DataObject::setReadObject(ReadObject* obj)
{
  // check if non empty
  if(opType_ != CommonCode::IOOpCode::OP_NOP)
  {
    // delete the current object before reseting the new one
    switch(opType_)
    {
      case CommonCode::IOOpCode::OP_WRITE:
      {
        delete ptr_.writePtr;
        ptr_.writePtr = nullptr;
        break;
      }
      case CommonCode::IOOpCode::OP_READ:
      {
        delete ptr_.readPtr;
        ptr_.readPtr = nullptr;
        break;
      }
  
      default:
      {
        LOG(WARNING) << "DataObject::opType_: "
                     << "neither OP_WIRTE nor OP_READ"; 
        break;
      }

    } //switch
    
  }//if


  opType_ = CommonCode::IOOpCode::OP_READ;
  ptr_.readPtr = obj;

}


WriteObject::WriteObject(UserCtx&& opCtx)
: useCtx_(std::move(opCtx)),
  validCtx_(true),
  opStat_(Status::ERR_INTERNAL)
{
}


WriteObject::WriteObject(WriteObject&& other)
: useCtx_(std::move(other.useCtx_)),
  validCtx_(other.validCtx_),
  opStat_(other.opStat_),
  buffer_(std::move(other.buffer_))
{}


WriteObject::~WriteObject()
{
  // clear all the data
  buffer_.clear();
}



uint64_t
WriteObject::availableBuffer() const
{

  if(isComplete())
  { // can take as much as possible
    return MAX_U64_T;
  }


  if(buffer_.length() >= MAX_RADOS_OP || !backUp_.empty())
  {
    return 0; // cannot append more
  }

  
  return static_cast<uint64_t>((MAX_RADOS_OP - buffer_.length()));

}


bool
WriteObject::appendBuffer(const char* addr, const uint64_t wLen)
{

  // check if can append that many data
  if(wLen > availableBuffer())
  {
    return false; // don't try to append
  }


  if(wLen > MAX_UINT_SIZE)
  { // use the back up buffer
    uint64_t remaining = wLen;
    const char* ptr = addr;
    
    if(buffer_.length() < MAX_RADOS_OP)
    { // append as much as possible to the buffer_
      const unsigned int canAppend = buffer_.length();
      buffer_.append(ptr, (MAX_RADOS_OP - canAppend));
      ptr += canAppend;
      remaining -= static_cast<uint64_t>(canAppend);
    }

    uint64_t bufferSize;
    unsigned int appendBack;
    librados::bufferlist& backRef = buffer_;
 
    do
    {
      backUp_.push_back(librados::bufferlist());
      backRef = backUp_.back();
      
      bufferSize = static_cast<uint64_t>(backRef.length());
      appendBack = (bufferSize < remaining) ? backRef.length() : static_cast<unsigned int>(remaining);
      
      backRef.append(ptr, appendBack);  
      ptr += appendBack;

      remaining -= static_cast<uint64_t>(appendBack);

    } while(remaining > 0); // keep pushing the values
   
   return true;
  }//if wLen > MAX_UINT_SIZE


  // otherwise can easily append
  buffer_.append(addr, static_cast<unsigned int>(wLen));

  return true;
}


bool
WriteObject::isComplete() const noexcept
{
  return (buffer_.length() == 0 && backUp_.empty());
}


librados::bufferlist&
WriteObject::getDataBuffer()
{
  if(buffer_.length() == 0 && !backUp_.empty())
  {
   // return the back up iterator
   LOG(WARNING) << "WriteObject::getDataBuffer: "
                << "buffer_.length() == 0 && !backUp_.empty()";
   return backUp_.front();
  }

  return buffer_;
}


const Task&
WriteObject::getTask() const
{
  return useCtx_.second;
}


Task&
WriteObject::getTask(const bool moveVal)
{
  if(validCtx_)
  {
    return useCtx_.second;
  }

  throw std::logic_error("Invalid User Context.");
}


const WriteObject::UserCtx&
WriteObject::getUserContext() const
{
  if(validCtx_)
  {
    return useCtx_;
  }

  throw std::logic_error("Invalid User Context.");

}


WriteObject::UserCtx&
WriteObject::getUserContext(const bool boolVal)
{
  if(validCtx_)
  {
    return useCtx_;
  }

  throw std::logic_error("Invalid User Context.");

}



void
WriteObject::updateDataBuffer(const uint64_t discard)
{

  const uint64_t buffSize = static_cast<const uint64_t>(buffer_.length());

  if(buffSize <= discard)
  {
    if(backUp_.empty())
    {
      buffer_.clear();
    }
    else
    { // move the front buffer to the buffer_
      buffer_.claim(backUp_.front(), 
                  librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);
      backUp_.pop_front(); // remove the front part
    }
    
  }
  else
  { // need to copy most of the current data
    const char* bufData = (buffer_.c_str() + discard);
    const unsigned int needCopy = static_cast<unsigned int>(buffSize - discard);

    // copy the data into a temp buffer
    librados::bufferlist tmpList;
    tmpList.append(bufData, needCopy);

    //claim all data
    buffer_.claim(tmpList, 
               librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);  
  }//else

}



bool
WriteObject::replaceUserContext(WriteObject::UserCtx&& newCtx)
{
  useCtx_ = std::move(newCtx);
  validCtx_ = true; // when replaced

  return true;
}


bool
WriteObject::setResponse(Task&& response)
{
  if(validCtx_)
  {
    useCtx_.first.setValue(std::move(response));
    validCtx_ = false;  
 
    return true;
  }

  return false;
}


ReadObject::ReadObject(ReadObject::UserCtx&& ctx)
: useCtx_(std::move(ctx)),
  opStat_(Status::ERR_INTERNAL),
  totalObjSize_(0),
  objOffset_(0),
  avData_(0)
{}


ReadObject::ReadObject(ReadObject&& other)
: useCtx_(std::move(other.useCtx_)),
  opStat_(other.opStat_),
  readData_(std::move(other.readData_)),
  totalObjSize_(other.totalObjSize_),
  objOffset_(other.objOffset_),
  avData_(other.avData_)
{}


ReadObject::~ReadObject()
{
  readData_.clear();
}



void
ReadObject::setObjectSize(const uint64_t size)
{

  // clear any previously
  // read objects
  readData_.clear();
  avData_ = 0;
  objOffset_ = 0;
  
  totalObjSize_ = size;

}

uint64_t
ReadObject::getObjectSize() const noexcept
{
  return totalObjSize_;
}


uint64_t
ReadObject::getRemainingObjectSize() const noexcept
{

  return (totalObjSize_ - objOffset_);

}


uint64_t
ReadObject::availableBuffer() const noexcept
{
  return avData_;
}


bool
ReadObject::appendBuffer(librados::bufferlist&& buffer)
{
  // either create a new part or 
  // append to the tail
  if((MAX_UINT_SIZE - buffer.length()) < readData_.back().rawData_.length())
  {
    //create a new part
    ReadObject::ObjectPart newPart;
    newPart.rawData_.claim(buffer, 
        librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);

    // enqueue it
    readData_.push_back(std::move(newPart));

  } //if
  else
  {
    //can append to the current tail
    readData_.back().rawData_.claim_append(buffer,
        librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);

  }//else


   // update the number of available bytes
   avData_ += static_cast<uint64_t>(buffer.length());

  return true;
}


bool
ReadObject::isComplete() const noexcept
{
  return (totalObjSize_ == objOffset_);
}

const Task&
ReadObject::getTask() const
{
  return useCtx_.second;

}

Task&
ReadObject::getTask(const bool moveVal)
{
  if(validCtx_)
  {
    return useCtx_.second;
  }

  throw std::logic_error("Invalid User Context");
}


const ReadObject::UserCtx&
ReadObject::getUserContext() const
{
  if(validCtx_)
  {
    return useCtx_;
  }

  throw std::logic_error("Invalid User Context.");
}


ReadObject::UserCtx&
ReadObject::getUserContext(const bool boolVal)
{
  if(validCtx_)
  {
    validCtx_ = false;

    return useCtx_;
  }

  throw std::logic_error("Inalid User Context.");

}



const char*
ReadObject::getRawBytes(uint64_t& readBytes)
{

  if(avData_ == 0 || readBytes == 0)
  {
    readBytes = 0;  //no data to read
    return nullptr;
  }


  // read the front item
  while(readData_.front().offset_ == readData_.front().rawData_.length())
  {
    // in order to avoid copying the data, it may happen
    // that there are some completed but not removed
    // bufferlists.
    readData_.pop_front();
  }


  auto& tmpFront =  readData_.front(); // reference to the front
  const uint64_t canRead = static_cast<const uint64_t>(tmpFront.rawData_.length() - tmpFront.offset_);

  unsigned int readOffset = 0; // for returning data
  
  // check if canRead is less than readBytes
  if(readBytes > canRead)
  {
    readBytes   = canRead;  // set the number of bytes to be returned
    avData_    -= canRead;  // update locally available bytes
    objOffset_ += canRead;  // total read bytes
    
    readOffset = tmpFront.offset_;
    
    // need to pop in the next request
    tmpFront.offset_ = tmpFront.rawData_.length();
  }
  else
  { // read a part of the front part
    avData_    -= readBytes;
    objOffset_ += readBytes;
    
    readOffset = tmpFront.offset_; // read offset position
     
    // update for next read
    tmpFront.offset_ += static_cast<unsigned int>(readBytes);
  }

  // return the address of raw data 
  return (tmpFront.rawData_.c_str() + readOffset);

}


bool
ReadObject::replaceUserContext(ReadObject::UserCtx&& newCtx)
{
  useCtx_ = std::move(newCtx);
  validCtx_ = true;

  return true;

}

bool
ReadObject::setResponse(Task&& response)
{
  if(validCtx_)
  {
    useCtx_.first.setValue(std::move(response));
    validCtx_ = false;

    return true;
  }

  return false;
}

} // namespace radosbuffermanagement 
} // namesapce singaistorageipc
