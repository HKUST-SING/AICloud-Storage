// C++ std lib
#include <cstring>

// Project lib
#include "StoreObj.h"

namespace singaistorageipc
{

StoreObj::StoreObj(const char* glKey, const uint32_t noShreds, 
                   const uint64_t totalSize)
: shreds_(noShreds),
  size_(totalSize),
  totalRead_(0),
  containData_(0)

{
  std::strcpy(globalId_, glKey);
}



StoreObj::StoreObj(StoreObj&& other)
: shreds_(other.shreds_),
  size_(other.size_),
  totalRead_(other.totalRead_),
  containData_(other.containData_),
  radosObjs_(std::move(other.radosObjs_))
{
  std::strcpy(globalId_, other.globalId_);
}
    

bool 
StoreObj::appendToShred(const uint32_t shredIdx, librados::bufferlist& bl)
{
  // first check if it is the last one
  if(radosObjs_.empty())
  {
    return false; // must create a shred first
  }

 
  // check if to the last shred to append
  if(radosObjs_.back().index_ == shredIdx)
  { // fast and convenient append
    radosObjs_.back().rawData_.claim_append(bl, 
               librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE); 
                                // potentially zero-copy append
   
    // update potential number of bytes to read
    containData_ += static_cast<uint64_t>(bl.length()); 
    return true; // successful append 
   
  }
  


  // look for the shred and try to append to its end
  for(auto& radObj : radosObjs_) // look for the index, and 
                                 // if it is found, append data
  {
    if(radObj.index_ == shredIdx)
    {
      radObj.rawData_.claim_append(bl, 
             librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE); 
                              // potentially zero-copy append

      // update length
      containData_ += static_cast<uint64_t>(bl.length());
      return true; // successful append
    }
  }

  return false; // cannot find the shred
}



bool
StoreObj::createShred(const std::string& shredKey, const uint32_t shredIdx,
                   librados::bufferlist& bl)
{
  // append the shred to back of the objects
  ObjShred tmpObj(shredIdx, shredKey); // create a new shred
  
  // append to the end
  radosObjs_.push_back(std::move(tmpObj)); // added data

  // append the raw data
  radosObjs_.back().rawData_.claim_append(bl, 
                  librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);


  // update the total number of available data
  containData_ += static_cast<uint64_t>(bl.length());

  return true; // success
}
 

char*
StoreObj::getRawBytes(uint64_t& needBytes)
{
  // read data sequentially and return bytes of a shred
  
  // start at the front

  if(containData_ == 0) // no data for reading
  {
    needBytes = 0;
    return nullptr;
  }  


  while(radosObjs_.front().offset_ == radosObjs_.front().rawData_.length())
  { // consider the front to be read
    // remove it
    radosObjs_.pop_front(); // pop the front and its data
    
  }

 
  auto& tmpFront = radosObjs_.front(); // get reference to the front
  const uint64_t canRead = static_cast<uint64_t>(tmpFront.rawData_.length() - tmpFront.offset_);

  unsigned int readOffset = 0; // for returning data

  // check if can read is larger than needBytes
  if(needBytes > canRead)
  {
    needBytes     = canRead; // set the number of bytes to return
    containData_ -= canRead; // update the number of available bytes
    totalRead_   += canRead;  // update the number of read bytes (total)

    readOffset   = tmpFront.offset_; // get read offset
    // need to pop in the next read call
    tmpFront.offset_ = tmpFront.rawData_.length();
    
    
  }
  else // read just a part of the front shred
  {
    containData_  -= needBytes; // update the number of available bytes
    totalRead_    += needBytes; // update the number of read bytes (total)
    
    readOffset  = tmpFront.offset_; // read offset
    // need to update the offset of the front shred
    tmpFront.offset_ += static_cast<unsigned int>(needBytes);
  }


  // return the address of data to read
  return (tmpFront.rawData_.c_str() + readOffset);
}
  

} // namesapce singaistorageipc
