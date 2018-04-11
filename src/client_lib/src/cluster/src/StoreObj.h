#pragma once

// C+ std lib
#include <list>
#include <utility>
#include <cstdint>


// Ceph lib
#include <rados/librados.hpp>


namespace singaistorageipc
{

class StoreObj
{
  /** 
   * Class that encapsulates a storage object as a set of Rados
   * objects. Since Rados objects have a size limit (~ 4GB), this
   * class encapsulates a larger object as a set of Rados objects.
   */

  public:


    typedef struct  ObjShred
    {
     /** 
      * Rados object which is a part of a larger storage object.
      *
      */
     uint32_t index_;               // index within the larger object
     std::string shredId_;          // rados key value
  

     librados::bufferlist rawData_; // read rados object data

     unsigned int offset_;          // how many bytes from 
                                    // the beginning of the data


      ObjShred(const uint32_t index, const std::string& key)
      : index_(index), offset_(0)
      {}

      ObjShred(struct ObjShred&& other)
      : index_(other.index_),   shredId_(std::move(other.shredId_)),
        offset_(other.offset_)

      {
        // claim raw bytes from the 'other' shred
        rawData_.claim(other.rawData_, 
        librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);
      }
     
    } ObjShred; // struct ObjShred


    StoreObj() = delete; 
    StoreObj(const char* glKey, const uint32_t noShreds, 
             const uint64_t totalSize);
    StoreObj(StoreObj&&);
    

    // ensure only one copy exists at a time
    StoreObj(const StoreObj&) = delete;
    StoreObj& operator=(const StoreObj&) = delete;
    


    bool appendToShred(const uint32_t shredIdx, librados::bufferlist& bl);
    /** 
     * Method provides an iterface to append data to an exisiting shred.
     *
     * @param: shredIdx: shred index within the storage object
     * @param: bl: data to append
     *
     * @return: if successfully appended; if false, the bufferlist
     *          remains untouched.
     */


    bool createShred(const std::string& shredKey, const uint32_t shredIdx,
                   librados::bufferlist& bl);
    /** 
     * Method for creating a new object shred (Rados object).
     * 
     * @param: shredKey: shred key (Rados key)
     * @param: shredIdx: shred index within this storage object
     * @param: bl: initial data for creating a new shred
     *
     * @return: if successfully created a shred; if false, the 
     *          bufferlist remains untouched.
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

   inline bool storeObjComplete() const
   /** 
    * Check if the entire object has already been read.
    *
    * @return: true if the entire storage object has beeen read;
    */
   
    {
      return (totalRead_ == size_);
    }


    inline const std::string& getGlobalObjectId() const
    /**
     * Get global object id
     *
     * @return: global object id
     */
    {
      return globalId_;
    }



  private:
    uint32_t shreds_;                 // total number of shreds that
                                      // make up one storage object

    uint64_t size_;                   // size of the storage object 
                                      // in bytes
    uint64_t totalRead_;              // total number of bytes read
                                      // from this object 


    uint64_t containData_;            // bytes of data that are available
                                      // for reading

    std::string globalId_;            // global storage object id

 
    std::list<StoreObj::ObjShred> radosObjs_;    
                                      // Rados objects that make up
                                      // this storage object

}; // class StoreOj

} // namesapce singaistorageipc
