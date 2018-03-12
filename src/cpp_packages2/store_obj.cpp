// C++ std lib
#include <algorithm>
#include <cstring>


// Project lib
#include "store_obj.h"

namespace singaistorageipc
{

StoreObj::StoreObj(const char* glKey, const uint32_t noShreds, 
                   const uint64_t totalSize)
: shreds_(noShreds),
  size_(totalSize),
  std::strcpy(globalId_, glKey),
  totalRead_(0),
  containData_(0)

{

}



StoreObj::StoreObj(StoreObj&& other)
: shreds_(other.shreds_),
  size_(other.size_),
  totalRead_(other.totalRead_),
  conatinData_(other.containData_),
  strcpy(globalId_, other.globalId_),
  radosObjs_(std::move(radosObjs_))
{}
    

bool 
StoreObj::appendToShred(const uint32_t shredIdx, librados::bufferlist& bl)
{
  // first check if it is the last one
  if()
}


    void createShred(const std::string& shredKey, const uint32_t shredIdx,
                   librados::bufferlist&& bl);
    /** 
     * Method for creating a new object shred (Rados object).
     * 
     * @param: shredKey: shred key (Rados key)
     * @param: shredIdx: shred index within this storage object
     * @param: bl: initial data for creating a new shred
     */
 

   char* getRawBytes(uint64_t& needBytes);
   /** 
    * Return the required number of bytes if possible and 
    * update the internal offsets.
    *
    * @param needBytes: number of bytes requested to read
    *
    * @return: pointer to the data and number of bytes returned
    */
  

   inline uint64_t availableData() const
   /**
    * Returns the number of bytes available for reading in total.
    */
   {
     return containData_;
   }

   inline uint64_t storeObjComplete() const
   /** 
    * Check if the entire object has already been read.
    *
    * @return: true if the entire storage object has beeen read;
    */
   
    {
      return (totalRead_ == size_);
    }


  private:
    uint32_t total_;                  // total number of shreds that
                                      // make up one storage object

    uint64_t size_;                   // size of the storage object 
                                      // in bytes
    uint64_t totalRead_;              // total number of bytes read
                                      // from this object 


    uint64_t containData_;            // bytes of data that are available
                                      // for reading

    const char[GLOBAL_ID_LENGTH];     // global storage object id
 
    std::vector<ObjShred> radosObjs_; // Rados objects that make up
                                      // this storage object

}; // class StoreOj

} // namesapce singaistorageipc
