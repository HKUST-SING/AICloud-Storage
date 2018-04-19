// C++ std lib
#include <cstring>
#include <limits>


// Project lib
#include "DataObj.h"



static constexpr const unsigned int MAX_RADOS_OP = (std::limits<unsigned int>::max() >> 1);

static constexpr const unsigned int MAX_UINT_SIZE = std::limits<unsigned int>::max();


namespace singaistorageipc
{

DataObject::DataObject()
: ptr_.writePtr{nullptr},
  ptr_.readPtr{nullptr},
  opType_(CommonCode::IOOpCode::OP_NOP)
{
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
      opType_        = CommonCode::IOOpCode::OP_NOP;
    }
      
  } // switch

  other.ptr_.writePtr = nullptr;
  other.ptr_.readPtr  = nullptr;
  other.opType_       = CommonCode::IOOpCode::OP_NOP;
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
DataObject::getObjectOpStatus() const
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


bool
DataObject::isComplete() const
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
        int a = 5; //need to log
        break;
      }

    } //switch
    
  }//if


  opType_ = CommonCode::IOOpCode::OP_WRITE;
  ptr_.writePtr = obj;

}


void
DataObject::setReadObject(Readobject* obj)
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
        int a = 5; //need to log
        break;
      }

    } //switch
    
  }//if


  opType_ = CommonCode::IOOpCode::OP_READ;
  ptr_.readPtr = obj;

}


WriteObject::WriteObject(UserCtx&& opCtx)
: useCtx_(std::move(opCtx)),
  opStat_(Status::ERR_INTERNAL)
{
}


WriteObject::WriteObject(WriteObject&& other)
: useCtx_(std::move(other.useCtx_)),
  opStat_(other.opStat_),
  buffer_(std::move(other.buffer_))
{}


WriteObject::~WriteObject()
{
  // clear all the data
  buffer_.clear();
}

void
WriteObject::setObjectOpStatus(const WriteObject::Status status)
{
  opStat_ = status;
}

uint64_t
WriteObject::availableBuffer() const
{

  if(buffer_.length() >= MAX_RADOS_OP)
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


  // otherwise can easily append
  buffer_.append(addr, static_cast<unsigned int>(wLen));

  return true;
}


bool
WriteObject::isComplete() const
{
  return (buffer_.length() == 0);
}


librados::bufferlist&
WriteObject::getDataBuffer()
{
  return buffer_;
}


WriteObject::UserCtx&
WriteObject::getUserContext()
{
  return useCtx_;
}



void
WriteObject::updateDataBuffer(const uint64_t discard)
{
  const uint64_t buffSize = static_cast<const uint64_t>(buffer_.length());

  if(buffSize <= discard)
  {
    buffer_.clear();
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
    
  }
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
ReadObject::setObjectOpStatus(const ReadObject::Status status)
{
  opStat_ = status;
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
ReadObject::getObjectSize() const
{
  return totalObjSize_;
}

uint64_t
ReadObject::availableBuffer() const
{
  return avData_;
}


bool
ReadObject::appenndBuffer(librados::bufferlist&& buffer)
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
    readData.push_back(std::move(newPart));

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
ReadObject::isComplete() const
{
  return (totalObjSize_ == objOffset_);
}

ReadObject::UserCtx&
ReadObject::getUserContext()
{
  return useCtx_;

}


char*
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

} // namesapce singaistorageipc
