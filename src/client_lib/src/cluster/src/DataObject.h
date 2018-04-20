#pragma once

// C+ std lib
#include <list>
#include <utility>
#include <cstdint>


// Facebook folly
#include <folly/futures/Promise.h>
#include <folly/futures/Future.h>


// Ceph librados
#include <rados/librados.hpp>


// Porject libraries
#include "include/Task.h"
#include "include/CommonCode.h"


namespace singaistorageipc
{


  class WriteObject
  {
    /**
     * Interface for write object handling.
     */ 

    protected:
     using UserCtx = std::pair<folly::Promise<Task>, Task>;
     using Status  = CommonCode::IOStatus;
     using OpType  = CommonCode::IOOpCode;
     using BackUp  = std::list<librados::bufferlist>;


    public:
      explicit WriteObject(UserCtx&& opCtx); 
      WriteObject(WriteObject&& other);

      WriteObject() = delete;
      WriteObject(const WriteObject&) = delete;
      WriteObject& operator=(const WriteObject&) = delete;
      WriteObject& operator=(WriteObject&&) = delete; 
      

      ~WriteObject(); // for future use make it virtual

    
      inline OpType getObjectOpType() const
      {
        return OpType::OP_WRITE;
      }


      inline Status getObjectOpStatus() const
      {
        return opStat_;
      }

      inline void setObjectOpStatus(const Status status)
      {
        opStat_ = status;
      }

      
      uint64_t availableBuffer() const;

      bool appendBuffer(const char* addr, const uint64_t len);


      bool isComplete() const;

      librados::bufferlist& getDataBuffer();

      const Task& getTask() const;

      Task& getTask(const bool);

      void updateDataBuffer(const uint64_t);

      bool replaceUserContext(UserCtx&& ctx);

      inline bool validUserContext() const
      {
        return validCtx_;
      }

      bool setResponse(Task&& response);

      UserCtx& getUserContext(const bool);

      const UserCtx& getUserContext() const;



    private:
      UserCtx              useCtx_;
      bool                 validCtx_;
      Status               opStat_;
      librados::bufferlist buffer_;
      BackUp               backUp_; // only for very
                                    // large writes



  }; // class WriteObject


  class ReadObject
  {
   
    protected:
  
      /**
       * A part of a larger storage object.
       */
 
      typedef struct ObjectPart
      {
        librados::bufferlist rawData_;
        unsigned int offset_;

  
        ObjectPart()
        : offset_(0)
        {}

        ObjectPart(struct ObjectPart&& other)
        : offset_(other.offset_)
        {
          // calim the buffer of 'other'
          rawData_.claim(other.rawData_,
                   librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);
        }    

        ~ObjectPart()
         {
           rawData_.clear();
         } 


        void appendData(librados::bufferlist&& buffer)
        {
          rawData_.claim_append(buffer, 
                   librados::bufferlist::CLAIM_ALLOW_NONSHAREABLE);
        } 
  

      } ObjectPart; // struct ObjectPart


 
      using UserCtx = std::pair<folly::Promise<Task>, Task>;
      using Status  = CommonCode::IOStatus;
      using OpType  = CommonCode::IOOpCode;

  
    public:
      explicit ReadObject(UserCtx&& ctx);
      ReadObject(UserCtx&& ctx, const uint64_t objSize);
      ReadObject(ReadObject&& other);

      ReadObject() = delete;
      ReadObject(const ReadObject&) = delete;
      ReadObject& operator=(const ReadObject&) = delete;
      ReadObject& operator=(ReadObject&&) = delete; 


      ~ReadObject(); // consider to make virtual in the future


      inline OpType getObjectOpType() const
      {
        return OpType::OP_READ;
      }


      inline Status getObjectOpStatus() const
      {
        return opStat_;
      }


      inline void setObjectOpStatus(const Status status)
      {
        opStat_ = status;
      }

      void setObjectSize(const uint64_t size);

      uint64_t getObjectSize() const;

      
      uint64_t availableBuffer() const;

      bool appendBuffer(librados::bufferlist&& buffer);


      bool isComplete() const;

      const char* getRawBytes(uint64_t& readBytes);

      const Task& getTask() const;

      Task& getTask(const bool);


      bool replaceUserContext(UserCtx&& ctx);

      inline bool validUserContext() const
      {
        return validCtx_;
      }

      bool setResponse(Task&& response);


      UserCtx& getUserContext(const bool);

      const UserCtx& getUserContext() const;


    private:
      UserCtx useCtx_;
      bool    validCtx_;
      Status  opStat_;
      std::list<ObjectPart> readData_;
      uint64_t totalObjSize_;
      uint64_t objOffset_;
      uint64_t avData_; // locally available 
 

  }; // class ReadObject



  class DataObject
  {

    /**
     * In order to avoid one shared class for 
     * read/write classes, this class is introduced.
     */
 
    private:
      typedef union 
      {
        WriteObject* writePtr;
        ReadObject*  readPtr;

      } ObjectPtr;     


      using UserCtx = std::pair<folly::Promise<Task>, Task>; 

    public:
      
      static const Task empty_task;     

      DataObject();
      DataObject(DataObject&&);
      ~DataObject();
 
   
      inline CommonCode::IOOpCode getObjectOpType() const
      {
        return opType_;
      }

      CommonCode::IOStatus getObjectOpStatus() const;

      void setObjectOpStatus(const CommonCode::IOStatus status);


      void setWriteObject(WriteObject* ptr);

      void setReadObject(ReadObject* ptr);

      bool isComplete() const;

      const Task& getTask() const;
      Task& getTask(const bool);
      bool replaceUserContext(UserCtx&& ctx);
      bool validUserContext() const;
      bool setResponse(Task&& response);


      WriteObject* getWriteObject(const bool claim=false);
      
      ReadObject* getReadObject(const bool claim=false);

      UserCtx& getUserContext(const bool);

      const UserCtx& getUserContext() const;


    private:
      ObjectPtr ptr_;
      CommonCode::IOOpCode opType_;
  
  

  }; // wraps read write operations

} // namesapce singaistorageipc
