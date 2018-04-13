#pragma once

// C++ std lib
#include <memory>


//Project lib
#include "CephContext.h"
#include "include/Task.h"
#include "include/CommonCode.h"


namespace singaistorageipc
{
class RemoteProtocol
{
  /**
   * The class provides an interface for communication between
   * a remote server and a local worker instance. Derived classes
   * should implement the interface and use their specific protocols,
   * however, they shoud\ld adhere to the interface so that 
   * a worker could communicate with the remote server without
   * much of the details.
   */




  public:

    /**
     * This class is an interface for operations handlers. 
     * A protocol takes a message, deciphers it and provides
     * operations and responses.
     */
    class ProtocolHandler
    {
      public:
        ProtocolHandler(CephContext& cephCont, 
                        const CommonCode::IOStatus opStat,
                        const CommonCode::IOOpCode opCode,
                        const bool needCeph = false)
        : cephCtx_(cephCont),
          ioStat_(opStat),
          opType_(opCode),
          needCeph_(needCeph)
        {}

        ProtocolHandler(const ProtocolHandler&) = delete;
        ProtocolHandler(ProtocolHandler&&) = delete;
        ProtocolHandler& operator=(const ProtocolHandler&) = delete;
        ProtocolHandler& operator=(ProtocolHandler&&) = delete;




        CommonCode::IOStatus getStatusCode() const
        {
          return ioStat_;
        }

        /**
         * Does this handler invovle Ceph operations
         */
        bool needCephIO() const
        {
          return needCeph_;
        } 

     
        /**
         * Need to notify about a successful Ceph operation.
         */
        virtual bool needNotifyCephSuccess() const
        {
          return false;
        }

        /**
         * Need to notify about a failed Ceph operation.
         */
        virtual bool needNotifyCephFailure() const
        {
          return false;
        }

        
        virtual std::unique_ptr<IOResult>  getCephSuccessResult()
        {
          return std::make_unique<IOResult>(nullptr);
        }


        virtual std::unique_ptr<IOResult> getCephFailureResult()
        {
          return std::make_unique<IOResult>(nullptr);
        }
   
        

        /**
         * Write data to a storage system object.
         * 
         * @param: rawData    : pointer to data
         * @param: writeBytes : bytes to write
         * @param: userCtx    : some user context which will be
         *                      returned with polling 
         *                      (by poling CephContext)
         *
         * @return : copied bytes (0 on failure)
         */
        virtual uint64_t writeData(const char*    rawData,
                                   const uint64_t writeBytes,
                                   void*          userCtx) = 0;




        /**
         * Read data from a storage system object.
         * 
         * @param: readBytes : bytes that the caller can read at most
         * @param: userCtx   : some user context which will be
         *                     returned with polling 
         *                     (by poling CephContext)
         *
         * @return : bytes to be read (0 on failure)
         */
        virtual uint64_t readData(const uint64_t readBytes,
                                  void*          userCtx) = 0;


        /** 
         * Only valid for read operations.
         *
         * @return: returns total size of the object.
         */

        virtual uint64_t getTotalDataSize() const
        {
          return 0;
        }


        CommonCode::IOOpCode getOpType() const
        {
          return opType_;
        }
        


      protected:
        CephContext& cephCtx_;
        const CommonCode::IOStatus ioStat_;
        const CommonCode::IOOpCode opType_;
        const bool needCeph_;
    
    }; //class ProtocolHandler





  public:
    RemoteProtocol() = default;
    RemoteProtocol(const RemoteProtocol&) = delete;
    RemoteProtocol(RemoteProtocol&&) = delete;
    RemoteProtocol& operator=(const RemoteProtocol&) = delete;
    RemoteProtocol& operator=(RemoteProtocol&&) = delete;

    virtual ~RemoteProtocol() = default; // shall be virtual

    
    /**
     * Initialize a protocol
     */
    inline bool initializeProtocol()
    {
      return doInitialize();
    }


    /**
     * Close the protocol. May release all allocated resources.
     */
    inline void closeProtocol()
    {
      doClose();
    }


    /**
     * Do request successes/failures have to be propagated
     * to the authentication server?
     */
    virtual bool needNotifyRequestSuccess() const
    {
      return false;
    }


    virtual bool needNotifyRequestFailure() const
    {
      return false;
    }


    virtual IOResult& getNotifySuccessMessage() 
    {
      // default implementation is 
      return emptyRes_; // return empty message
    }

    
    virtual IOResult& getNotifyFailureMessage()
    {
      return emptyRes_; // return empty message
    }



    /**
     * This is the meat of the protocol. It takes an IOResponse,
     * tries to decipher it and prepare all messages.
     */

    virtual std::shared_ptr<RemoteProtocol::ProtocolHandler> 
                 handleMessage(
                     std::shared_ptr<IOResponse> ioRes,
                     const Task& task,
                     CephContext& cephCtx) = 0;
    



  protected:
   
    /**
     * Derived classes implement this for initialization
     */
     virtual bool doInitialize() = 0;

    /** 
     * Derived classes implement this for closing.
     */
     virtual void doClose() = 0;
 



  protected:
    IOResult emptyRes_; // empty result (can be resused many times
                        // since move operations make it valid) 
  
  


}; // class RemoteProtocol

} // namesapce singaistorageipc
