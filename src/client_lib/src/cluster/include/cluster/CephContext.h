#pragma once

// C++ std lib
#include <map>
#include <string>
#include <cstdint>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>


// Ceph lib
#include <rados/librados.hpp>


//Project lib
#include "include/ConcurrentQueue.h"
#include "include/CommonCode.h"


namespace singaistorageipc
{


// Define a function for Rados callbacks 
extern void aioRadosCallbackFunc(librados::completion_t cb, void* args);


class CephContext
{
/**
 * Ceph context maintains state information to access a Ceph 
 * cluster. It is used by workers in order to handle IO 
 * operations from local machines to the Ceph cluster.
 * Each worker has its own instance of CephContext to avoid
 * state sharing.
 */



  public:
    typedef struct RadosOpCtx
    {
      CommonCode::IOOpCode   opType;   // operation opcode
      CommonCode::IOStatus   opStatus; // operation status

      librados::bufferlist   opData;   // data (for read only)
      void*                  userCtx;  // user context (passed together)

      RadosOpCtx()
      : opType(CommonCode::IOOpCode::OP_NOP),
        opStatus(CommonCode::IOStatus::ERR_INTERNAL),
        userCtx(nullptr)
      {
      }


      /* No Copying is supported */
      RadosOpCtx(const RadosOpCtx&) = delete;
      RadosOpCtx& operator=(const RadosOpCtx&) = delete;
      RadosOpCtx& operator=(RadosOpCtx&&) = delete;


      /* Move constructor is supported */
      RadosOpCtx(RadosOpCtx&& other)
      : opType(other.opType),
        opStatus(other.opStatus),
        opData(std::move(opData)),
        userCtx(other.userCtx)
      {
        // reset the user context of 'other'
        other.userCtx = nullptr;
      }


     bool release()
     {
       //if(userCtx) // must reset context
      // {
      //   return false; // signal that context is still available
      // }
      
       opData.clear(); // clear the data

       delete this; // delete the object
     
       return true;
       
     }

     unsigned int getDataLength() const noexcept
     {
       return opData.length();
     }

     
     static RadosOpCtx* createRadosOpCtx() 
     {
       return new RadosOpCtx(); 
     }


     private:

       // users cannot delete the context (use release method)
       ~RadosOpCtx() = default;


    } RadosOpCtx; // struct RadosOpCtx


  private:

    // Friend function to access the private class
    friend void aioRadosCallbackFunc(librados::completion_t cb,
                                         void* args); 

    class RadosOpHandler
    {

      private:

        // friend function for accessing provate fields 
        // of the class
        friend void aioRadosCallbackFunc(librados::completion_t cb,
                                         void* args); 

        typedef struct CmplCtx
        {
          RadosOpHandler*          objPtr;   // object pointer
          librados::AioCompletion* ioCmpl;   // completion context  
          RadosOpCtx*              radosCtx; // rados op context

          
          CmplCtx(RadosOpHandler* objPtr = nullptr,
                  librados::AioCompletion* aioRados = nullptr,
                  RadosOpCtx* opCtx = nullptr)
          : ioCmpl(aioRados),
            radosCtx(opCtx)
          {}

          CmplCtx(const CmplCtx&) = delete;
          CmplCtx(CmplCtx&&) = delete;
          CmplCtx& operator=(const CmplCtx&) = delete;
          CmplCtx& operator=(CmplCtx&&) = delete;



          static CmplCtx* createHandlerCompletionContext()
          {
            return new CmplCtx();
          }

          void release()
          {
            if(ioCmpl)
            {
              ioCmpl->release(); // deletes itself
              ioCmpl = nullptr;
            }
            
            if(radosCtx)
            {
              // reset the context
              radosCtx->userCtx = nullptr;
              radosCtx->release(); // deletes itself
              radosCtx = nullptr;
            }

            // reset the object pointer
            objPtr = nullptr;

            delete this; // delete the object

          }

         private:
           ~CmplCtx() = default;

        } CmplCtx; // struct CmplCtx

        /**
         * Simple class which is reponsible for handling Rados
         * requests. This implementation uses async operations,
         * but in general it is preferred to have an interface
         * an multiple implementations.
         */
    

     public:
        RadosOpHandler()  = delete;
        explicit RadosOpHandler(librados::Rados* rados) noexcept;
        ~RadosOpHandler();


        /**
         * Read an object from the cluster.
         *
         * @param: ioCtx:     pool context
         * @param: objId:     Rados object id
         * @param: readBytes: how many bytes to read
         * @param: offset:    from where to start reading  
         * @param: userCtx:   user passed context which will be returned
         *
         * @return: number of bytes to be read (0 on failure) 
         */
        size_t readRadosObject(librados::IoCtx* ioCtx,
                               const std::string& objId,
                               const size_t readBytes,
                               const uint64_t offset=0,
                               void* userCtx=nullptr);

        /**
         * Write data to the cluster (a Rados object).
         * 
         *
         * @param: ioCtx:      pool context
         * @param: objId:      Rados object id
         * @param: rawData:    data to write to an object 
         * @param: writeBytes: size of the data buffer
         * @param: offset:     offset at which to write
         * @param: append:     append the data to the current object
         * @param: userCtx:    user context which is returned 
         *                     with this call
         *
         * @return : number of bytes to be written (0 on failure)
         */
        size_t writeRadosObject(librados:: IoCtx* ioCtx,
                                const std::string& objId,
                                const char* rawData,
                                const size_t  writeBytes,
                                const uint64_t offset=0,
                                const bool append=true,
                                void* userCtx=nullptr);


        /**
         * Write data to the cluster (a Rados object).
         * 
         *
         * @param: ioCtx:      pool context
         * @param: objId:      Rados object id
         * @param: buffer:     data to write to an object 
         * @param: writeBytes: size of the data buffer to write
         * @param: offset:     offset at which to write
         * @param: append:     append the data to the current object
         * @param: userCtx:    user context which is returned 
         *                     with this call
         *
         * @return : number of bytes to be written (0 on failure)
         */
        size_t writeRadosObject(librados:: IoCtx* ioCtx,
                                const std::string& objId,
                                const librados::bufferlist& buffer,
                                const size_t  writeBytes,
                                const uint64_t offset=0,
                                const bool append=true,
                                void* userCtx=nullptr);


        /**
         * Poll the handler for completed Async Rados IO
         * operations.
         *
         * @param: completeAIOs: for storing all completed operations.
         */
        void pollAsyncIOs(std::deque<CephContext::RadosOpCtx*>& 
                               completedAIOs);



       /**
        * Ensure that the handler completes all its operations
        * or at least tries its best to clean up properly.
        */
       void stopRadosOpHandler();
     


        private:
         /**
          * Private method which is called every time an ASYNC
          * operation completes.
          */
          void radosAsyncCompletionCallback(librados::completion_t cb,
                                            void* args);


         /** 
          * Clear any pending AIOs
          */
          void clearPendingAIOs();
      
      

        private:
          // atomic data for ensuring that all operations
          // have completed
          std::atomic<unsigned int>                activeIOs_;
          std::atomic<bool>                        done_;

          // for terminating the handler only
          std::mutex                               termLock_;
          std::condition_variable                  termCond_;           

          librados::Rados*                         cluster_;
       
 
          ConcurrentQueue<CephContext::RadosOpCtx*,
                          std::deque<CephContext::RadosOpCtx*> >  
                                                   asyncOps_; 
                                              // asynchronously finished
                                              // operations
    

    }; // class RadosOpHandler




  public:
    
     // Public return size definition
     using PollSize = std::deque<CephContext::RadosOpCtx*>::size_type;


    CephContext() = delete; // no dfault constructor
    ~CephContext();
    CephContext& operator=(const CephContext&) = delete;
    CephContext(const CephContext&);   // support copy constructor
    CephContext(CephContext&&)=delete; // rados does not support

    CephContext(const char* conf, std::string&& userName, 
                std::string&& clusterName, const uint64_t flags);

    CephContext(const char* conf, const std::string& userName, 
                const std::string& clusterName, const uint64_t flags);
 
    
    bool initAndConnect();
    /**
     * Initializes the ceph context. Must be called from the same thread
     * after construction. The method initialized the cluster handle
     * and tries to connect to the cluster.
     * 
     *
     * @return: true on success, false on failure.
     */


   inline  bool checkPoolExists(const std::string& poolName) const
   /**
    * Check if the pool name has already been added.
    *
    * @param: poolName: name of the pool in the Ceph cluster
    *
    * @return: true if the pool exists, false if not.
    */
   {
     return (ioOps_.find(poolName) != ioOps_.end());
   }



  inline librados::IoCtx& getPoolRadosContext(const std::string& poolName)
  {
    /** 
     * Only use this method if checking is done before.
     */

    return (ioOps_.find(poolName)->second);
  }



   inline bool addRadosContext(const std::string& poolName)
   {
    // such pool does not exists yet
     if(ioOps_.find(poolName) == ioOps_.end())
     {
       return connectRadosContext(poolName);
     }
     
     return false; // means there is such a rados context 
            
   }

 
  inline bool removeRadosContext(const std::string&  poolName)
  // remove the pol context from the current connections
  {
    auto itr = ioOps_.find(poolName);
    if(itr != ioOps_.end())
    { // proceed further
    
      closeRadosContext(poolName);
      ioOps_.erase(itr); // remove from the map

      return true;
    }


    return false; // no such pool
  }


  void closeCephContext();
  /**
   * Method is called when the user wants to close the cluster 
   * connection. After this call, all io operations fail.
   *
   */
    


  /** 
   * Read data from the underlying data cluster (Ceph).
   * Data is returned by polling the context and asking
   * for any completed operations.
   * 
   * @param: oid       : Rados object unique id
   * @param: poolName  : Ceph pool name
   * @param: readBytes : number of bytes to read
   * @param: offset    : where to start reading the object
   * @param: userCtx   : user context which will be returned 
   *                     when poll for responses
   *
   * @return: returns number of bytes to be read (0 on failure)
   */
  size_t readObject(const std::string& oid,  
                    const std::string& poolName,
                    const size_t readBytes, 
                    const uint64_t offset=0,
                    void* userCtx = nullptr);




  /** 
   * Write data to the underlying data cluster (Ceph).
   * Result is returned by polling the context and asking
   * for any completed operations.
   * 
   * @param: oid        : Rados object unique id
   * @param: poolName   : Ceph pool name
   * @param: buffer     : data to write to the cluster
   * @param: writeBytes : number of bytes to write
   * @param: offset     : at which offset of the object to write to
   * @param: userCtx    : user context which will be returned 
   *                      when poll for responses
   *
   * @return: number of bytes to be written to the cluster
   *          (0 on failure)
   */
    size_t writeObject(const std::string& oid, 
                       const std::string& poolName,
                       const char* buffer, 
                       const size_t writeBytes, 
                       const uint64_t offset=0,
                       const bool append = true, 
                       void* userCtx = nullptr);


    /** 
   * Write data to the underlying data cluster (Ceph).
   * Result is returned by polling the context and asking
   * for any completed operations.
   * 
   * @param: oid        : Rados object unique id
   * @param: poolName   : Ceph pool name
   * @param: buffer     : data to write to the cluster
   * @param: writeBytes : number of bytes to write
   * @param: offset     : at which offset of the object to write to
   * @param: userCtx    : user context which will be returned 
   *                      when poll for responses
   *
   * @return: number of bytes to be written to the cluster
   *          (0 on failure)
   */
    size_t writeObject(const std::string& oid, 
                       const std::string& poolName,
                       const librados::bufferlist& buffer, 
                       const size_t writeBytes, 
                       const uint64_t offset=0,
                       const bool append = true, 
                       void* userCtx = nullptr); 


  /**
   * Poll for any completed previously issued IO operations.
   * The return structures keep references to the client
   * contexts (userCtx).
   *
   * @param: ops : a deque for storing completed ops
   * 
   * @return: returns the number of retrieved completed ops
   */
    PollSize getCompletedOps(
       std::deque<CephContext::RadosOpCtx*>& ops);



  private:
    bool connectRadosContext(const std::string& poolName);
    /**
     * Tries to connect to a remote Ceph pool and if it succeeds,
     * adds the newly created context to the pool.
     *
     * @param: Ceph poolname
     *
     * @return: true on success, false on failure.
     */

    bool closeRadosContext(const std::string& poolName);
    /** 
     * Closes a rados contex. Need to explicitly remove 
     * from the map.
     * 
     * @param: ceph poolname
     *
     * @return: true on success, false in failure.
     */
   

  private:
    librados::Rados cluster_; // cluster handle (one at most)
    std::string clusterName_; // cluster name
    std::string accessName_;  // username used for accessing 
                              //data in cluster

    const char* confFile_;    // Ceph configuration file

    uint64_t accessFlags_;    // access flags (Ceph specific)
    std::map<std::string, librados::IoCtx> ioOps_; // IO handles 
                                                   // (one per pool)

    bool init_;           // flag if the cluster initialized
    RadosOpHandler* opHandler_; // entity responseible for
                                // IO operations    
 
}; // class CephContext


} // namesapce singaistorageipc
